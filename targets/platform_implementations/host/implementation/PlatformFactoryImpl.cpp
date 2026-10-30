#include "targets/platform_implementations/host/implementation/PlatformFactoryImpl.hpp"
#include "core/platform_abstraction/AdcPhaseCurrentMeasurement.hpp"
#include <cstdlib>

namespace application
{
    PlatformFactoryImpl::PlatformFactoryImpl(const infra::Function<void()>& onInitialized)
        : onInitialized(onInitialized)
    {
        this->onInitialized();
    }

    void PlatformFactoryImpl::Run()
    {
        eventDispatcher.Run();
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
        return ledPins.operational;
    }

    hal::GpioPin& PlatformFactoryImpl::WarningLed()
    {
        return ledPins.warning;
    }

    hal::GpioPin& PlatformFactoryImpl::FailureLed()
    {
        return ledPins.failure;
    }

    uint8_t PlatformFactoryImpl::BoardId() const
    {
        return 0;
    }

    hal::Hertz PlatformFactoryImpl::SystemClock() const
    {
        return hal::Hertz(0);
    }

    foc::Volts PlatformFactoryImpl::PowerSupplyVoltage()
    {
        return foc::Volts(48.0f);
    }

    foc::Ampere PlatformFactoryImpl::MaxCurrentSupported() const
    {
        return foc::Ampere(5.0f);
    }

    foc::LowPriorityInterrupt& PlatformFactoryImpl::LowPriorityInterrupt()
    {
        return simpleLowPriorityInterrupt;
    }

    void PlatformFactoryImpl::Start()
    {
    }

    void PlatformFactoryImpl::ConfigureAdcAndPwm(hal::Hertz baseFrequency, std::chrono::nanoseconds /*deadTime*/, SampleAndHold /*sampleAndHold*/)
    {
        phaseCurrentAdc.reset();
        phaseCurrentAdc.emplace(1.0f, 0.0f);
        pwm.reset();
        pwm.emplace();
        pwmBaseFrequency = baseFrequency;
    }

    void PlatformFactoryImpl::SetEncoderResolution(uint32_t resolution)
    {
        encoder.reset();
        encoder.emplace(resolution);
    }

    void PlatformFactoryImpl::ConfigureCanBus(uint32_t /*bitRate*/, bool /*testMode*/)
    {
        canBus.reset();
        canBus.emplace();
        diagnostics.AttachCanBus(*canBus);
    }

    CanBusAdapter& PlatformFactoryImpl::CanBus()
    {
        return *canBus;
    }

    void PlatformFactoryImpl::PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents)>& onDone)
    {
        onPhaseCurrentsReady = onDone;
        phaseCurrentAdc->Measure([this](foc::Ampere a, foc::Ampere b, foc::Ampere c)
            {
                if (onPhaseCurrentsReady)
                    onPhaseCurrentsReady(foc::PhaseCurrents{ a, b, c });
            });
    }

    void PlatformFactoryImpl::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        if (pwm)
            pwm->Start(dutyPhases.a, dutyPhases.b, dutyPhases.c);
    }

    void PlatformFactoryImpl::Stop()
    {
        if (pwm)
            pwm->Stop();
    }

    hal::Hertz PlatformFactoryImpl::BaseFrequency() const
    {
        return pwmBaseFrequency;
    }

    foc::Radians PlatformFactoryImpl::Read()
    {
        if (!encoder)
            return foc::Radians{ 0.0f };
        return encoder->Read() - encoderOffset;
    }

    void PlatformFactoryImpl::Set(foc::Radians value)
    {
        if (encoder)
            encoderOffset = encoder->Read() - value;
    }

    void PlatformFactoryImpl::SetZero()
    {
        if (encoder)
            encoderOffset = encoder->Read();
    }

    hal::Eeprom& PlatformFactoryImpl::Eeprom()
    {
        return eepromStub;
    }

    void PlatformFactoryImpl::Reset()
    {
        std::exit(0);
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
