#include "targets/platform_implementations/host/implementation/PlatformFactoryImpl.hpp"
#include "core/platform_abstraction/AdcPhaseCurrentMeasurement.hpp"
#include <cstdlib>

namespace application
{
    PlatformFactoryImpl::PlatformFactoryImpl(const infra::Function<void()>& onInitialized)
        : onInitialized(onInitialized)
    {}

    void PlatformFactoryImpl::Run()
    {
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
        return pin;
    }

    hal::GpioPin& PlatformFactoryImpl::WarningLed()
    {
        return pin;
    }

    hal::GpioPin& PlatformFactoryImpl::FailureLed()
    {
        return pin;
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
        return *this;
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

    uint32_t PlatformFactoryImpl::ElapsedCycles()
    {
        return 0;
    }

    void PlatformFactoryImpl::Trigger()
    {
        simpleLowPriorityInterrupt.Trigger();
    }

    void PlatformFactoryImpl::Register(const infra::Function<void()>& handler)
    {
        simpleLowPriorityInterrupt.Register(handler);
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
    }

    CanBusAdapter& PlatformFactoryImpl::CanBus()
    {
        return *canBus;
    }

    void PlatformFactoryImpl::PhaseCurrentsReady(hal::Hertz /*baseFrequency*/, const infra::Function<void(foc::PhaseCurrents)>& onDone)
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

    void PlatformFactoryImpl::SerialCommunicationStub::SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion)
    {}

    void PlatformFactoryImpl::SerialCommunicationStub::ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived)
    {}

    bool PlatformFactoryImpl::GpioPinStub::Get() const
    {
        return false;
    }

    void PlatformFactoryImpl::GpioPinStub::Set(bool value)
    {}

    bool PlatformFactoryImpl::GpioPinStub::GetOutputLatch() const
    {
        return false;
    }

    void PlatformFactoryImpl::GpioPinStub::SetAsInput()
    {}

    bool PlatformFactoryImpl::GpioPinStub::IsInput() const
    {
        return false;
    }

    void PlatformFactoryImpl::GpioPinStub::Config(hal::PinConfigType config)
    {}

    void PlatformFactoryImpl::GpioPinStub::Config(hal::PinConfigType config, bool startOutputState)
    {}

    void PlatformFactoryImpl::GpioPinStub::ResetConfig()
    {}

    void PlatformFactoryImpl::GpioPinStub::EnableInterrupt(const infra::Function<void()>& action, hal::InterruptTrigger trigger, hal::InterruptType type)
    {}

    void PlatformFactoryImpl::GpioPinStub::DisableInterrupt()
    {}

    void PlatformFactoryImpl::AdcMultiChannelStub::Measure(const infra::Function<void(Samples)>& onDone)
    {
        onDone(Samples());
    }

    void PlatformFactoryImpl::AdcMultiChannelStub::Stop()
    {
    }

    void PlatformFactoryImpl::ThreeChannelsPwmStub::SetBaseFrequency(hal::Hertz baseFrequency)
    {
    }

    void PlatformFactoryImpl::ThreeChannelsPwmStub::Stop()
    {
    }

    void PlatformFactoryImpl::ThreeChannelsPwmStub::Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2, hal::Percent dutyCycle3)
    {
    }

    uint32_t PlatformFactoryImpl::SynchronousQuadratureEncoderStub::Position()
    {
        return 0;
    }

    uint32_t PlatformFactoryImpl::SynchronousQuadratureEncoderStub::Resolution()
    {
        return 0;
    }

    hal::SynchronousQuadratureEncoder::MotionDirection PlatformFactoryImpl::SynchronousQuadratureEncoderStub::Direction()
    {
        return MotionDirection::forward;
    }

    uint32_t PlatformFactoryImpl::SynchronousQuadratureEncoderStub::Speed()
    {
        return 0;
    }

    void PlatformFactoryImpl::CanStub::SendData(Id, const Message&, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        actionOnCompletion(true);
    }

    void PlatformFactoryImpl::CanStub::ReceiveData(const infra::Function<void(Id id, const Message& data)>&)
    {
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
}
