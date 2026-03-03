#include "source/hardware/st/implementation/HardwareFactoryImpl.hpp"
#include "infra/util/MemoryRange.hpp"
#include DEVICE_HEADER

namespace
{
    infra::Function<void()>* pendSvHandlerCallback = nullptr;
}

extern "C" void PendSV_Handler()
{
    if (pendSvHandlerCallback != nullptr && *pendSvHandlerCallback)
        (*pendSvHandlerCallback)();
}

unsigned int hse_value = 8'000'000;

namespace application
{
    HardwareFactoryImpl::HardwareFactoryImpl(const infra::Function<void()>& onInitialized)
        : onInitialized(onInitialized)
    {
        HAL_Init();
    }

    void HardwareFactoryImpl::Run()
    {
    }

    services::Tracer& HardwareFactoryImpl::Tracer()
    {
        return terminalAndTracer.tracer;
    }

    services::TerminalWithCommands& HardwareFactoryImpl::Terminal()
    {
        return terminalAndTracer.terminal;
    }

    infra::MemoryRange<hal::GpioPin> HardwareFactoryImpl::Leds()
    {
        return infra::MakeRangeFromSingleObject(pin);
    }

    hal::PerformanceTracker& HardwareFactoryImpl::PerformanceTimer()
    {
        return *this;
    }

    hal::Hertz HardwareFactoryImpl::SystemClock() const
    {
        return hal::Hertz(SystemCoreClock);
    }

    foc::Volts HardwareFactoryImpl::PowerSupplyVoltage()
    {
        return foc::Volts(48.0f);
    }

    foc::Ampere HardwareFactoryImpl::MaxCurrentSupported()
    {
        return foc::Ampere(15.0f);
    }

    foc::LowPriorityInterrupt& HardwareFactoryImpl::LowPriorityInterrupt()
    {
        return pendSvLowPriorityInterrupt;
    }

    void HardwareFactoryImpl::PendSvLowPriorityInterrupt::Trigger()
    {
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    }

    void HardwareFactoryImpl::PendSvLowPriorityInterrupt::Register(const infra::Function<void()>& handler)
    {
        onLowPriorityInterrupt = handler;
        pendSvHandlerCallback = &onLowPriorityInterrupt;
    }

    void HardwareFactoryImpl::Start()
    {
    }

    uint32_t HardwareFactoryImpl::ElapsedCycles()
    {
        return 0;
    }

    infra::CreatorBase<hal::SynchronousThreeChannelsPwm, void(std::chrono::nanoseconds deadTime, hal::Hertz frequency)>& HardwareFactoryImpl::SynchronousThreeChannelsPwmCreator()
    {
        return pwmBrushless;
    }

    infra::CreatorBase<AdcPhaseCurrentMeasurement, void(HardwareFactory::SampleAndHold)>& HardwareFactoryImpl::AdcMultiChannelCreator()
    {
        return adcCurrentPhases;
    }

    infra::CreatorBase<QuadratureEncoderDecorator, void()>& HardwareFactoryImpl::SynchronousQuadratureEncoderCreator()
    {
        return synchronousQuadratureEncoderCreator;
    }

    void HardwareFactoryImpl::SerialCommunicationStub::SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion)
    {}

    void HardwareFactoryImpl::SerialCommunicationStub::ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived)
    {}

    bool HardwareFactoryImpl::GpioPinStub::Get() const
    {
        return false;
    }

    void HardwareFactoryImpl::GpioPinStub::Set(bool value)
    {}

    bool HardwareFactoryImpl::GpioPinStub::GetOutputLatch() const
    {
        return false;
    }

    void HardwareFactoryImpl::GpioPinStub::SetAsInput()
    {}

    bool HardwareFactoryImpl::GpioPinStub::IsInput() const
    {
        return false;
    }

    void HardwareFactoryImpl::GpioPinStub::Config(hal::PinConfigType config)
    {}

    void HardwareFactoryImpl::GpioPinStub::Config(hal::PinConfigType config, bool startOutputState)
    {}

    void HardwareFactoryImpl::GpioPinStub::ResetConfig()
    {}

    void HardwareFactoryImpl::GpioPinStub::EnableInterrupt(const infra::Function<void()>& action, hal::InterruptTrigger trigger, hal::InterruptType type)
    {}

    void HardwareFactoryImpl::GpioPinStub::DisableInterrupt()
    {}

    void HardwareFactoryImpl::AdcMultiChannelStub::Measure(const infra::Function<void(Samples)>& onDone)
    {
        onDone(Samples());
    }

    void HardwareFactoryImpl::AdcMultiChannelStub::Stop()
    {
    }

    void HardwareFactoryImpl::SynchronousThreeChannelsPwmStub::SetBaseFrequency(hal::Hertz baseFrequency)
    {
    }

    void HardwareFactoryImpl::SynchronousThreeChannelsPwmStub::Stop()
    {
    }

    void HardwareFactoryImpl::SynchronousThreeChannelsPwmStub::Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2, hal::Percent dutyCycle3)
    {
    }

    uint32_t HardwareFactoryImpl::SynchronousQuadratureEncoderStub::Position()
    {
        return 0;
    }

    uint32_t HardwareFactoryImpl::SynchronousQuadratureEncoderStub::Resolution()
    {
        return 0;
    }

    hal::SynchronousQuadratureEncoder::MotionDirection HardwareFactoryImpl::SynchronousQuadratureEncoderStub::Direction()
    {
        return MotionDirection::forward;
    }

    uint32_t HardwareFactoryImpl::SynchronousQuadratureEncoderStub::Speed()
    {
        return 0;
    }
}
