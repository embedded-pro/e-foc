#include "targets/platform_implementations/peripheral_stubs/PeripheralStubs.hpp"

namespace application
{
    void SerialCommunicationStub::SendData(infra::ConstByteRange, infra::Function<void()>)
    {}

    void SerialCommunicationStub::ReceiveData(infra::Function<void(infra::ConstByteRange)>)
    {}

    bool GpioPinStub::Get() const
    {
        return false;
    }

    void GpioPinStub::Set(bool)
    {}

    bool GpioPinStub::GetOutputLatch() const
    {
        return false;
    }

    void GpioPinStub::SetAsInput()
    {}

    bool GpioPinStub::IsInput() const
    {
        return false;
    }

    void GpioPinStub::Config(hal::PinConfigType)
    {}

    void GpioPinStub::Config(hal::PinConfigType, bool)
    {}

    void GpioPinStub::ResetConfig()
    {}

    void GpioPinStub::EnableInterrupt(const infra::Function<void()>&, hal::InterruptTrigger, hal::InterruptType)
    {}

    void GpioPinStub::DisableInterrupt()
    {}

    void AdcMultiChannelStub::Measure(const infra::Function<void(Samples)>& onDone)
    {
        onDone(Samples(neutralSamples));
    }

    void AdcMultiChannelStub::Stop()
    {
    }

    void ThreeChannelsPwmStub::SetBaseFrequency(hal::Hertz)
    {
    }

    void ThreeChannelsPwmStub::Stop()
    {
    }

    void ThreeChannelsPwmStub::Start(hal::DutyCycle, hal::DutyCycle, hal::DutyCycle)
    {
    }

    uint32_t SynchronousQuadratureEncoderStub::Position()
    {
        return 0;
    }

    uint32_t SynchronousQuadratureEncoderStub::Resolution()
    {
        return 0;
    }

    hal::SynchronousQuadratureEncoder::MotionDirection SynchronousQuadratureEncoderStub::Direction()
    {
        return MotionDirection::forward;
    }

    uint32_t SynchronousQuadratureEncoderStub::Speed()
    {
        return 0;
    }

    void CanStub::SendData(Id, const Message&, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        actionOnCompletion(true);
    }

    void CanStub::ReceiveData(const infra::Function<void(Id id, const Message& data)>&)
    {
    }
}
