#include "tools/virtual_motor/platform/VirtualPlatform.hpp"
#include "generated/Boards.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include "tools/virtual_motor/platform/Pacer.hpp"
#include <array>
#include <cstdlib>
#include <thread>

namespace virtual_motor
{
    namespace
    {
        constexpr uint32_t defaultBaseFrequencyHz{ 20000 };
        constexpr std::array<uint32_t, 5> sampleHoldClocks{ 4, 16, 32, 64, 256 };

        sil::PlantDriveDefaults Defaults()
        {
            return { foc::M_2310P_LN_04K::parameters, foc::M_2310P_LN_04K::ratedSupply, hal::Hertz{ defaultBaseFrequencyHz } };
        }
    }

    VirtualPlatform::VirtualPlatform(const BoardOptions& options)
        : options(options)
        , drive(options.plant, Defaults())
        , eeprom(options.memoryFile)
        , canPort(options.hubPort)
    {
        boardProtection.Configure({
            .overCurrentAmpere = options.plant.overCurrentTripAmpere,
            .overVoltageVolts = options.plant.overVoltageTripVolts,
            .underVoltageVolts = options.plant.underVoltageTripVolts,
            .overTemperatureCelsius = options.plant.overTemperatureTripCelsius,
        });
        services::SetGlobalTracerInstance(console.tracer);
        diagnostics.AttachCanBus(canPort);
    }

    void VirtualPlatform::AdvanceOneMillisecond()
    {
        const auto ticks = drive.BaseFrequency().Value() / 1000;
        for (uint32_t tick = 0; tick != ticks; ++tick)
            ControlTick();

        boardProtection.DeliverPendingProtection();
        execution.timerService.SystemTickInterrupt();
        if (eachMillisecond)
            eachMillisecond();
        canPort.Poll();
        execution.eventDispatcher.ExecuteAllActions();
    }

    void VirtualPlatform::ControlTick()
    {
        drive.Tick();

        if (onPhaseCurrentsReady && !controlLoopEntered)
        {
            controlLoopEntered = true;
            onPhaseCurrentsReady(drive.ShuntCurrents());
            controlLoopEntered = false;
        }

        auto& model = drive.Model();
        boardProtection.Evaluate(model.CurrentSensor().Last(), model.Supply().Voltage().Value(), model.Thermal().Temperature());
        execution.lowPriorityInterrupt.RunIfTriggered();
    }

    void VirtualPlatform::EachMillisecond(const infra::Function<void()>& action)
    {
        eachMillisecond = action;
    }

    void VirtualPlatform::Run()
    {
        Pacer pacer{ []()
            {
                return Pacer::Clock::now();
            },
            [this]()
            {
                AdvanceOneMillisecond();
            } };

        while (true)
        {
            pacer.Service();
            std::this_thread::sleep_until(pacer.NextDue());
        }
    }

    void VirtualPlatform::ConfigureAdcAndPwm(hal::Hertz, std::chrono::nanoseconds, SampleAndHold sampleAndHold)
    {
        drive.SampleAndHoldClocks(sampleHoldClocks.at(static_cast<std::size_t>(sampleAndHold)));
    }

    drivers::DeadTimeLoss VirtualPlatform::InverterDeadTimeLoss(std::chrono::nanoseconds) const
    {
        return drive.InverterDeadTimeLoss();
    }

    uint32_t VirtualPlatform::DutyUpdateDelayPeriods() const
    {
        return drive.DutyUpdateDelayPeriods();
    }

    void VirtualPlatform::SetEncoderResolution(uint32_t)
    {}

    void VirtualPlatform::ConfigureCanBus(uint32_t bitRate, bool)
    {
        canPort.Connect(options.bus.c_str(), bitRate);
    }

    services::MonitoredCan& VirtualPlatform::CanBus()
    {
        return canPort;
    }

    services::Tracer& VirtualPlatform::Tracer()
    {
        return console.tracer;
    }

    services::TerminalWithCommands& VirtualPlatform::Terminal()
    {
        return console.terminal;
    }

    hal::GpioPin& VirtualPlatform::OperationalLed()
    {
        return leds.operational;
    }

    hal::GpioPin& VirtualPlatform::WarningLed()
    {
        return leds.warning;
    }

    hal::GpioPin& VirtualPlatform::FailureLed()
    {
        return leds.failure;
    }

    uint8_t VirtualPlatform::BoardId() const
    {
        return options.node;
    }

    application::BoardKind VirtualPlatform::GetBoardKind() const
    {
        return static_cast<application::BoardKind>(e_foc::boards::virtualBoard.kind);
    }

    hal::Hertz VirtualPlatform::SystemClock() const
    {
        return hal::Hertz{ 0 };
    }

    foc::Volts VirtualPlatform::PowerSupplyVoltage()
    {
        return drive.Model().Supply().Voltage();
    }

    foc::LowPriorityInterrupt& VirtualPlatform::LowPriorityInterrupt()
    {
        return execution.lowPriorityInterrupt;
    }

    hal::Eeprom& VirtualPlatform::Eeprom()
    {
        return eeprom;
    }

    void VirtualPlatform::RegisterBoardProtection(const infra::Function<void(BoardProtectionReason)>& onProtection)
    {
        boardProtection.Register(onProtection);
    }

    application::PlatformFactory::BoardProtectionState VirtualPlatform::BoardProtectionStatus()
    {
        return boardProtection.Status();
    }

    application::PlatformDiagnostics& VirtualPlatform::Diagnostics()
    {
        return diagnostics;
    }

    void VirtualPlatform::Reset()
    {
        if (options.onReset)
            options.onReset();
        std::exit(EXIT_SUCCESS);
    }

    application::ResetCause VirtualPlatform::GetResetCause() const
    {
        return options.restarted ? application::ResetCause::software : application::ResetCause::powerUp;
    }

    infra::BoundedConstString VirtualPlatform::FaultStatus() const
    {
        return {};
    }

    std::optional<std::reference_wrapper<drivers::EncoderIndex>> VirtualPlatform::EncoderIndexChannel()
    {
        return *this;
    }

    void VirtualPlatform::PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents)>& onDone)
    {
        onPhaseCurrentsReady = onDone;
    }

    void VirtualPlatform::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        drive.Output(dutyPhases);
    }

    void VirtualPlatform::Start()
    {
        drive.Start();
        boardProtection.SetArmed(true);
    }

    void VirtualPlatform::Stop()
    {
        boardProtection.SetArmed(false);
        drive.Stop();
    }

    hal::Hertz VirtualPlatform::BaseFrequency() const
    {
        return drive.BaseFrequency();
    }

    foc::Ampere VirtualPlatform::MaxCurrentSupported() const
    {
        return plantModel.MaxCurrentSupported();
    }

    void VirtualPlatform::CurrentOffsets(const foc::PhaseCurrents& offsets)
    {
        drive.CurrentOffsets(offsets);
    }

    foc::Radians VirtualPlatform::Read()
    {
        return drive.Model().Encoder().Read();
    }

    void VirtualPlatform::Set(foc::Radians value)
    {
        drive.Model().Encoder().Set(value);
    }

    void VirtualPlatform::SetZero()
    {
        drive.Model().Encoder().SetZero();
    }

    bool VirtualPlatform::HasIndex() const
    {
        return plantEncoder.HasIndex();
    }

    void VirtualPlatform::ArmIndex()
    {
        drive.ArmIndex();
    }

    bool VirtualPlatform::IndexSeen()
    {
        return drive.IndexSeen();
    }
}
