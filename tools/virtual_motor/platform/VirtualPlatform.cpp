#include "tools/virtual_motor/platform/VirtualPlatform.hpp"
#include "generated/Boards.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include "tools/virtual_motor/platform/Pacer.hpp"
#include <cstdlib>
#include <thread>

namespace virtual_motor
{
    VirtualPlatform::VirtualPlatform(const BoardOptions& options)
        : PlantPowerStage(options.plant)
        , options(options)
        , eeprom(options.memoryFile)
        , canPort(options.hubPort)
    {
        services::SetGlobalTracerInstance(console.tracer);
        diagnostics.AttachCanBus(canPort);
    }

    void VirtualPlatform::AdvanceOneMillisecond()
    {
        const auto ticks = BaseFrequency().Value() / 1000;
        for (uint32_t tick = 0; tick != ticks; ++tick)
        {
            StepPlant();
            execution.lowPriorityInterrupt.RunIfTriggered();
        }

        DeliverPendingProtection();
        execution.timerService.SystemTickInterrupt();
        if (eachMillisecond)
            eachMillisecond();
        canPort.Poll();
        execution.eventDispatcher.ExecuteAllActions();
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

    foc::LowPriorityInterrupt& VirtualPlatform::LowPriorityInterrupt()
    {
        return execution.lowPriorityInterrupt;
    }

    hal::Eeprom& VirtualPlatform::Eeprom()
    {
        return eeprom;
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
}
