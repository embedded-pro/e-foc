#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "targets/platform_implementations/qemu/implementation/PlatformFactoryImpl.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include <chrono>
#include <cstdio>

namespace application
{
    PlatformFactoryImpl::PlatformFactoryImpl(const foc::ThreePhaseMotorModel::Parameters& motorParams,
                                             const infra::Function<void()>& onInit)
        : onInitialized(onInit)
        , model(
              motorParams,
              foc::Volts{ 48.0f },
              hal::Hertz{ 20000 },
              std::nullopt,
              false)
    {
        cortex.systemTick.Start();
        services::SetGlobalTracerInstance(terminalAndTracer.tracer);
        onInitialized();
        focTimer.Start();
    }

    void PlatformFactoryImpl::Run()
    {
        std::puts("READY");
        std::fflush(stdout);

        while (true)
            cortex.eventDispatcher.ExecuteAllActions();
    }

    services::Tracer& PlatformFactoryImpl::Tracer()
    {
        return terminalAndTracer.tracer;
    }

    services::TerminalWithCommands& PlatformFactoryImpl::Terminal()
    {
        return terminalAndTracer.terminal;
    }

    hal::GpioPin& PlatformFactoryImpl::OperationalLed()
    {
        return operationalPin;
    }

    hal::GpioPin& PlatformFactoryImpl::WarningLed()
    {
        return warningPin;
    }

    hal::GpioPin& PlatformFactoryImpl::FailureLed()
    {
        return failurePin;
    }

    uint8_t PlatformFactoryImpl::BoardId() const
    {
        return 0;
    }

    bool PlatformFactoryImpl::PowerStatus() const
    {
        return true;
    }

    hal::PerformanceTracker& PlatformFactoryImpl::PerformanceTimer()
    {
        return performanceTracker;
    }

    hal::Hertz PlatformFactoryImpl::SystemClock() const
    {
        return hal::Hertz{ kQemuSystemClockHz };
    }

    foc::Volts PlatformFactoryImpl::PowerSupplyVoltage()
    {
        return foc::Volts{ 48.0f };
    }

    foc::LowPriorityInterrupt& PlatformFactoryImpl::LowPriorityInterrupt()
    {
        return pendSvLowPriorityInterrupt;
    }

    hal::Eeprom& PlatformFactoryImpl::Eeprom()
    {
        return eeprom;
    }

    void PlatformFactoryImpl::RegisterBoardProtection(const infra::Function<void(BoardProtectionReason)>&)
    {}

    void PlatformFactoryImpl::Reset()
    {}

    ResetCause PlatformFactoryImpl::GetResetCause() const
    {
        return ResetCause::powerUp;
    }

    infra::BoundedConstString PlatformFactoryImpl::FaultStatus() const
    {
        return {};
    }

    void PlatformFactoryImpl::ConfigureAdcAndPwm(hal::Hertz freq, std::chrono::nanoseconds, SampleAndHold)
    {
        baseFrequency = freq;
    }

    void PlatformFactoryImpl::SetEncoderResolution(uint32_t)
    {}

    void PlatformFactoryImpl::ConfigureCanBus(uint32_t, bool)
    {
        if (!canBusAdapter)
        {
            canBusAdapter.emplace();
            canPollTimer.Start(std::chrono::milliseconds(1), [this]()
            {
                canBusAdapter->PollIncoming();
            });
        }
    }

    CanBusAdapter& PlatformFactoryImpl::CanBus()
    {
        return *canBusAdapter;
    }

    OPTIMIZE_FOR_SPEED void PlatformFactoryImpl::PhaseCurrentsReady(hal::Hertz freq, const infra::Function<void(foc::PhaseCurrents)>& onDone)
    {
        baseFrequency = freq;
        onPhaseCurrentsReady = onDone;
        model.PhaseCurrentsReady(freq, [this](foc::PhaseCurrents currents)
            {
                lastCurrents = currents;
            });
    }

    OPTIMIZE_FOR_SPEED void PlatformFactoryImpl::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        lastDutyPhases = dutyPhases;
    }

    void PlatformFactoryImpl::FocTimerIsr()
    {
        model.StepForTest(lastDutyPhases);
        if (onPhaseCurrentsReady)
            onPhaseCurrentsReady(lastCurrents);
    }

    void PlatformFactoryImpl::Start()
    {
        model.Start();
    }

    void PlatformFactoryImpl::Stop()
    {
        model.Stop();
    }

    hal::Hertz PlatformFactoryImpl::BaseFrequency() const
    {
        return baseFrequency;
    }

    foc::Ampere PlatformFactoryImpl::MaxCurrentSupported() const
    {
        return model.MaxCurrentSupported();
    }

    OPTIMIZE_FOR_SPEED foc::Radians PlatformFactoryImpl::Read()
    {
        return model.Read();
    }

    void PlatformFactoryImpl::Set(foc::Radians value)
    {
        model.Set(value);
    }

    void PlatformFactoryImpl::SetZero()
    {
        model.SetZero();
    }
}
