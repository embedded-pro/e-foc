#include "targets/platform_implementations/ti/implementation/PlatformFactoryImpl.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include "targets/platform_implementations/error_handling_cortex_m/CutPowerStage.hpp"
#include "targets/platform_implementations/error_handling_cortex_m/PersistentFaultData.hpp"
#include DEVICE_HEADER
#include <atomic>
#include <cmath>

namespace
{
    static constexpr uint32_t sysctlRescExt = 0x00000001u;
    static constexpr uint32_t sysctlRescBor = 0x00000004u;
    static constexpr uint32_t sysctlRescWdt0 = 0x00000008u;
    static constexpr uint32_t sysctlRescSw = 0x00000010u;
    static constexpr uint32_t sysctlRescWdt1 = 0x00000020u;

    application::ResetCause ReadAndClearResetCause()
    {
        const uint32_t resc = SYSCTL->RESC;
        SYSCTL->RESC = 0u;

        if (resc & sysctlRescSw)
            return application::ResetCause::software;
        if (resc & sysctlRescWdt0)
            return application::ResetCause::watchdog;
        if (resc & sysctlRescWdt1)
            return application::ResetCause::watchdog;
        if (resc & sysctlRescBor)
            return application::ResetCause::brownOut;
        if (resc & sysctlRescExt)
            return application::ResetCause::hardware;
        return application::ResetCause::powerUp;
    }
}

namespace application
{
    namespace
    {
        bool ExceedsOverCurrent(foc::Ampere a, foc::Ampere b, foc::Ampere c)
        {
            constexpr float limit{ BoardCharacteristics::overcurrentThresholdAmps };

            return std::abs(a.Value()) > limit || std::abs(b.Value()) > limit || std::abs(c.Value()) > limit;
        }
    }

    extern "C" uint32_t SystemCoreClock;

    PlatformFactoryImpl::PlatformFactoryImpl(const infra::Function<void()>& onInitialized)
        : onInitialized(onInitialized)
        , resetCause(ReadAndClearResetCause())
    {
        if (persistentFaultData.IsValid())
        {
            FormatFaultData(persistentFaultData, faultStatusString);
            persistentFaultData.Invalidate();
        }

        ConfigureBoardIdPins();
        application::Clocks::Initialize();
        peripherals.emplace();
        boardId = ReadBoardIdPins();
        services::SetGlobalTracerInstance(peripherals->terminalAndTracer.tracer);
        this->onInitialized();
    }

    void PlatformFactoryImpl::Run()
    {
        peripherals->cortex.eventDispatcher.Run();
    }

    services::Tracer& PlatformFactoryImpl::Tracer()
    {
        return peripherals->terminalAndTracer.tracer;
    }

    services::TerminalWithCommands& PlatformFactoryImpl::Terminal()
    {
        return peripherals->terminalAndTracer.terminal;
    }

    hal::GpioPin& PlatformFactoryImpl::OperationalLed()
    {
        return Pins::operationalLed;
    }

    hal::GpioPin& PlatformFactoryImpl::WarningLed()
    {
        return Pins::warningLed;
    }

    hal::GpioPin& PlatformFactoryImpl::FailureLed()
    {
        return Pins::failureLed;
    }

    uint8_t PlatformFactoryImpl::BoardId() const
    {
        return boardId;
    }

    void PlatformFactoryImpl::ConfigureBoardIdPins()
    {
        if constexpr (Peripheral::hasBoardIdPins)
        {
            Pins::boardId0.Config(hal::PinConfigType::input);
            Pins::boardId1.Config(hal::PinConfigType::input);
            Pins::boardId2.Config(hal::PinConfigType::input);
        }
    }

    uint8_t PlatformFactoryImpl::ReadBoardIdPins()
    {
        if constexpr (!Peripheral::hasBoardIdPins)
            return 0;

        const uint8_t bit0 = Pins::boardId0.Get() ? 0u : 1u;
        const uint8_t bit1 = Pins::boardId1.Get() ? 0u : 1u;
        const uint8_t bit2 = Pins::boardId2.Get() ? 0u : 1u;
        return static_cast<uint8_t>(bit0 | (bit1 << 1u) | (bit2 << 2u));
    }

    hal::Hertz PlatformFactoryImpl::SystemClock() const
    {
        return hal::Hertz(SystemCoreClock);
    }

    foc::Volts PlatformFactoryImpl::PowerSupplyVoltage()
    {
        auto samples = peripherals->adcForPowerSupplyMeasurementImpl.powerSupplyAdc.Measure(1);

        return foc::Volts{ static_cast<float>(samples.front()) * AdcForPowerSupplyMeasurementImpl::adcToVoltsFactor };
    }

    foc::Ampere PlatformFactoryImpl::MaxCurrentSupported() const
    {
        return foc::Ampere(BoardCharacteristics::maxCurrentSupportedAmps);
    }

    drivers::DeadTimeLoss PlatformFactoryImpl::InverterDeadTimeLoss(std::chrono::nanoseconds deadTime) const
    {
        if (deadTime != std::chrono::nanoseconds{ BoardCharacteristics::measuredDeadBandNanoseconds })
            return {};

        return { std::chrono::nanoseconds{ BoardCharacteristics::effectiveDeadTimeNanoseconds }, foc::Ampere{ BoardCharacteristics::deadTimeKneeAmps } };
    }

    uint32_t PlatformFactoryImpl::DutyUpdateDelayPeriods() const
    {
        return 1;
    }

    foc::LowPriorityInterrupt& PlatformFactoryImpl::LowPriorityInterrupt()
    {
        return pendSvLowPriorityInterrupt;
    }

    hal::tiva::WatchDog::Config PlatformFactoryImpl::Cortex::WatchdogConfig()
    {
        hal::tiva::WatchDog::Config config;
        config.timeout = watchdogEarlyWarningPeriod;
        config.resetOnMissedInterrupt = true;
        config.interruptPriority = InterruptPriorities::watchdogEarlyWarning;
        return config;
    }

    void PlatformFactoryImpl::Cortex::OnWatchdogExpired()
    {
        CutPowerStage();
    }

    void PlatformFactoryImpl::ConfigureAdcAndPwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime, SampleAndHold sampleAndHold)
    {
        const auto periodCycles = baseFrequency.Value() == 0
                                      ? 0u
                                      : static_cast<uint32_t>(SystemCoreClock / baseFrequency.Value());
        controlLoopMetrics.Configure(static_cast<uint32_t>(static_cast<uint64_t>(periodCycles) * 3u / 4u), periodCycles);

        ReconfigureAdc(sampleAndHold);
        ReconfigurePwm(baseFrequency, deadTime);
        pwmBaseFrequency = baseFrequency;
    }

    void PlatformFactoryImpl::ReconfigureAdc(SampleAndHold sampleAndHold)
    {
        auto& impl = peripherals->adcForPhaseCurrentMeasurementImpl;
        auto& adcCfg = impl.adcConfig;
        adcCfg.interruptPriority = InterruptPriorities::phaseCurrentAdc;
        adcCfg.sampleAndHold = impl.toSampleAndHold.at(static_cast<std::size_t>(sampleAndHold));
        if constexpr (Peripheral::hasFaultComparators)
            adcCfg.digitalComparators = infra::MakeRange(impl.digitalComparators);

        peripherals->phaseCurrentAdc.reset();
        peripherals->phaseCurrentAdc.emplace(
            AdcForPhaseCurrentMeasurementImpl::adcToAmpereSlope,
            AdcForPhaseCurrentMeasurementImpl::adcToAmpereOffset,
            Peripheral::AdcIndex,
            Peripheral::AdcSequencerIndex,
            impl.currentPhaseAnalogPins,
            adcCfg);
    }

    void PlatformFactoryImpl::OnPwmFault(hal::tiva::Pwm::FaultEvent event)
    {
        if (!onFaultCallback)
            return;

        const auto bits = static_cast<uint8_t>(event.comparatorInputsByGenerator[1]);

        if (bits & static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator0))
            onFaultCallback(PlatformFactory::BoardProtectionReason::overCurrent);
        if (bits & static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator1))
            onFaultCallback(PlatformFactory::BoardProtectionReason::overVoltage);
        if (bits & static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator2))
            onFaultCallback(PlatformFactory::BoardProtectionReason::underVoltage);
    }

    void PlatformFactoryImpl::ReconfigurePwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime)
    {
        peripherals->pwm.reset();

        auto& cfg = peripherals->pwmConfig;
        const auto deadTimeCycles = PwmDriver::CalculateDeadTimeCycles(deadTime, cfg.clockDivisor);
        cfg.deadTimeConfig.fallInClockCycles = deadTimeCycles;
        cfg.deadTimeConfig.riseInClockCycles = deadTimeCycles;
        cfg.pwmConfig.deadTime = std::make_optional(cfg.deadTimeConfig);

        EmplacePwm(cfg);

        peripherals->pwm->SetBaseFrequency(baseFrequency);
    }

    void PlatformFactoryImpl::SetEncoderResolution(uint32_t resolution)
    {
        using Conf = hal::tiva::QuadratureEncoder::Config;
        hal::tiva::QuadratureEncoder::Config qeiConfig{ resolution, 0, Peripheral::encoderInvertPhaseA, false, false,
            Conf::ResetMode::onMaxPosition, Conf::CaptureMode::phaseAandPhaseB, Conf::SignalMode::quadrature };
        peripherals->encoder.reset();
        peripherals->encoder.emplace(resolution, Peripheral::QeiIndex, Pins::encoderA, Pins::encoderB, Pins::encoderZ, qeiConfig);
    }

    void PlatformFactoryImpl::ConfigureCanBus(uint32_t bitRate, bool testMode)
    {
        hal::tiva::Can::Config canConfig;
        canConfig.timing = hal::tiva::Can::BitRate{ bitRate };
        canConfig.testMode = testMode;
        canConfig.interruptPriority = InterruptPriorities::can;

        peripherals->canBus.reset();
        peripherals->canBus.emplace(
            Peripheral::CanIndex,
            Pins::canRx,
            Pins::canTx,
            canConfig,
            infra::Function<void(hal::tiva::Can::Error)>([this](hal::tiva::Can::Error error)
                {
                    peripherals->canBus->InvokeErrorHandler(ToAdapterError(error));
                }));

        diagnostics.AttachCanBus(*peripherals->canBus);
    }

    CanBusAdapter& PlatformFactoryImpl::CanBus()
    {
        return *peripherals->canBus;
    }

    void PlatformFactoryImpl::PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents)>& onDone)
    {
        onPhaseCurrentsReady.Set(onDone);
        peripherals->phaseCurrentAdc->Measure([this](foc::Ampere a, foc::Ampere b, foc::Ampere c)
            {
                if (controlLoopEntered)
                {
                    controlLoopMetrics.RecordReentry();
                    return;
                }

                controlLoopEntered = true;
                const auto entryCycles = CycleCounter::Now();

                const auto currents = shuntReadout.Read(foc::PhaseCurrents{ a, b, c });

                if (overCurrentTripped || ExceedsOverCurrent(currents.a, currents.b, currents.c))
                    TripOverCurrent();
                else
                    onPhaseCurrentsReady(currents);

                controlLoopMetrics.Record(CycleCounter::Now() - entryCycles);
                controlLoopEntered = false;
            });
    }

    void PlatformFactoryImpl::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        if (overCurrentTripped)
            return;

        const auto readable = foc::KeepLowSideShuntsReadable(dutyPhases, Peripheral::maxReadableDuty, Peripheral::maxDuty);
        shuntReadout.Applied(readable);

        WithPwm([&readable](auto& pwm)
            {
                pwm.Start(readable.a, readable.b, readable.c);
            });

        if (overCurrentTripped)
            CutPowerStage();
    }

    void PlatformFactoryImpl::Start()
    {
        if (overCurrentTripped)
            return;

        WithPwm([](auto& pwm)
            {
                pwm.Start(hal::DutyCycle::FromPercent(1), hal::DutyCycle::FromPercent(1), hal::DutyCycle::FromPercent(1));
            });

        if (overCurrentTripped)
            CutPowerStage();
    }

    void PlatformFactoryImpl::Stop()
    {
        WithPwm([](auto& pwm)
            {
                pwm.Stop();
            });
        overCurrentTripped = false;
    }

    void PlatformFactoryImpl::TripOverCurrent()
    {
        CutPowerStage();

        if (overCurrentTripped)
            return;

        overCurrentTripped = true;
        infra::EventDispatcher::Instance().Schedule([this]()
            {
                if (onFaultCallback)
                    onFaultCallback(PlatformFactory::BoardProtectionReason::overCurrent);
            });
    }

    hal::Hertz PlatformFactoryImpl::BaseFrequency() const
    {
        return pwmBaseFrequency;
    }

    // The encoder exists only once the NVM load has supplied its resolution
    foc::Radians PlatformFactoryImpl::Read()
    {
        if (!peripherals->encoder)
            return foc::Radians{ 0.0f };
        return encoderZero.AngleOf(peripherals->encoder->Read());
    }

    void PlatformFactoryImpl::Set(foc::Radians value)
    {
        if (peripherals->encoder)
            encoderZero.Set(peripherals->encoder->Read(), value);
    }

    void PlatformFactoryImpl::SetZero()
    {
        if (peripherals->encoder)
            encoderZero.SetZero(peripherals->encoder->Read());
    }

    hal::Eeprom& PlatformFactoryImpl::Eeprom()
    {
        return peripherals->eepromPeripheral;
    }

    void PlatformFactoryImpl::RegisterBoardProtection(const infra::Function<void(PlatformFactory::BoardProtectionReason)>& onProtection)
    {
        onFaultCallback = onProtection;
    }

    PlatformFactory::BoardProtectionState PlatformFactoryImpl::BoardProtectionStatus()
    {
        return PlatformFactory::BoardProtectionState::unknown;
    }

    void PlatformFactoryImpl::Reset()
    {
        NVIC_SystemReset();
    }

    ResetCause PlatformFactoryImpl::GetResetCause() const
    {
        return resetCause;
    }

    infra::BoundedConstString PlatformFactoryImpl::FaultStatus() const
    {
        return faultStatusString;
    }

    PlatformDiagnostics& PlatformFactoryImpl::Diagnostics()
    {
        return diagnostics;
    }
}
