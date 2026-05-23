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

    foc::Ampere PlatformFactoryImpl::MaxCurrentSupported() const
    {
        return foc::Ampere(15.0f);
    }

    foc::LowPriorityInterrupt& PlatformFactoryImpl::LowPriorityInterrupt()
    {
        return pendSvLowPriorityInterrupt;
    }

    void PlatformFactoryImpl::Trigger()
    {
        pendSvLowPriorityInterrupt.Trigger();
    }

    void PlatformFactoryImpl::Register(const infra::Function<void()>& handler)
    {
        pendSvLowPriorityInterrupt.Register(handler);
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

    void PlatformFactoryImpl::ConfigureAdcAndPwm(hal::Hertz baseFrequency, std::chrono::nanoseconds /*deadTime*/, SampleAndHold /*sampleAndHold*/)
    {
        phaseCurrentAdc.reset();
        phaseCurrentAdc.emplace(0.0f, 0.0f);
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
        phaseCurrentAdc->Measure([onDone](foc::Ampere a, foc::Ampere b, foc::Ampere c)
            {
                onDone(foc::PhaseCurrents{ a, b, c });
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
