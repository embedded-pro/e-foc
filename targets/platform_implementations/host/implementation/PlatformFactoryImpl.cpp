#include "targets/platform_implementations/host/implementation/PlatformFactoryImpl.hpp"
#include "core/platform_abstraction/AdcPhaseCurrentMeasurement.hpp"
#include "infra/util/MemoryRange.hpp"
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

    infra::MemoryRange<hal::GpioPin> PlatformFactoryImpl::Leds()
    {
        return infra::MakeRangeFromSingleObject(pin);
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

    foc::Ampere PlatformFactoryImpl::MaxCurrentSupported()
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

    infra::CreatorBase<hal::SynchronousThreeChannelsPwm, void(std::chrono::nanoseconds deadTime, hal::Hertz frequency)>& PlatformFactoryImpl::SynchronousThreeChannelsPwmCreator()
    {
        return pwmBrushless;
    }

    infra::CreatorBase<AdcPhaseCurrentMeasurement, void(PlatformFactory::SampleAndHold)>& PlatformFactoryImpl::AdcMultiChannelCreator()
    {
        return adcCurrentPhases;
    }

    infra::CreatorBase<QuadratureEncoderDecorator, void()>& PlatformFactoryImpl::SynchronousQuadratureEncoderCreator()
    {
        return synchronousQuadratureEncoderCreator;
    }

    infra::CreatorBase<CanBusAdapter, void(uint32_t bitRate, bool testMode)>& PlatformFactoryImpl::CanBusCreator()
    {
        return canCreator;
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

    void PlatformFactoryImpl::SynchronousThreeChannelsPwmStub::SetBaseFrequency(hal::Hertz baseFrequency)
    {
    }

    void PlatformFactoryImpl::SynchronousThreeChannelsPwmStub::Stop()
    {
    }

    void PlatformFactoryImpl::SynchronousThreeChannelsPwmStub::Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2, hal::Percent dutyCycle3)
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
