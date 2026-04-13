#include "targets/platform_implementations/st/implementation/PlatformFactoryImpl.hpp"
#include "infra/util/MemoryRange.hpp"
#include "targets/platform_implementations/error_handling_cortex_m/PersistentFaultData.hpp"
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

namespace
{
    application::ResetCause ReadAndClearResetCause()
    {
#if defined(STM32H5)
        const uint32_t rst = RCC->RSR;
        RCC->RSR |= RCC_RSR_RMVF;
        if (rst & RCC_RSR_SFTRSTF)
            return application::ResetCause::software;
        if (rst & (RCC_RSR_IWDGRSTF | RCC_RSR_WWDGRSTF))
            return application::ResetCause::watchdog;
        if (rst & RCC_RSR_BORRSTF)
            return application::ResetCause::brownOut;
        if (rst & RCC_RSR_PINRSTF)
            return application::ResetCause::hardware;
#elif defined(STM32F4)
        const uint32_t rst = RCC->CSR;
        RCC->CSR |= RCC_CSR_RMVF;
        if (rst & RCC_CSR_SFTRSTF)
            return application::ResetCause::software;
        if (rst & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF))
            return application::ResetCause::watchdog;
        if (rst & RCC_CSR_BORRSTF)
            return application::ResetCause::brownOut;
        if (rst & RCC_CSR_PINRSTF)
            return application::ResetCause::hardware;
#endif
        return application::ResetCause::powerUp;
    }
}

namespace application
{
    PlatformFactoryImpl::PlatformFactoryImpl(const infra::Function<void()>& onInitialized)
        : onInitialized(onInitialized)
        , resetCause(ReadAndClearResetCause())
    {
        if (persistentFaultData.IsValid())
        {
            FormatFaultData(persistentFaultData, faultStatusString);
            persistentFaultData.Invalidate();
        }

        HAL_Init();
    }

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
        return hal::Hertz(SystemCoreClock);
    }

    foc::Volts PlatformFactoryImpl::PowerSupplyVoltage()
    {
        return foc::Volts(48.0f);
    }

    foc::Ampere PlatformFactoryImpl::MaxCurrentSupported()
    {
        return foc::Ampere(15.0f);
    }

    foc::LowPriorityInterrupt& PlatformFactoryImpl::LowPriorityInterrupt()
    {
        return pendSvLowPriorityInterrupt;
    }

    void PlatformFactoryImpl::PendSvLowPriorityInterrupt::Trigger()
    {
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    }

    void PlatformFactoryImpl::PendSvLowPriorityInterrupt::Register(const infra::Function<void()>& handler)
    {
        onLowPriorityInterrupt = handler;
        pendSvHandlerCallback = &onLowPriorityInterrupt;
    }

    void PlatformFactoryImpl::Start()
    {
    }

    uint32_t PlatformFactoryImpl::ElapsedCycles()
    {
        return 0;
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
}
