#include "targets/platform_implementations/ti/implementation/PlatformFactoryImpl.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/tracer/GlobalTracer.hpp"
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

namespace
{
    // TM4C129x TRM Table 5-9: SYSCTL RESC register bit positions (offset 0x05C)
    static constexpr uint32_t sysctlRescExt = 0x00000001u;  // Bit 0: External reset pin
    static constexpr uint32_t sysctlRescBor = 0x00000004u;  // Bit 2: Brown-Out reset
    static constexpr uint32_t sysctlRescWdt0 = 0x00000008u; // Bit 3: Watchdog Timer 0
    static constexpr uint32_t sysctlRescSw = 0x00000010u;   // Bit 4: Software reset (NVIC_SystemReset)
    static constexpr uint32_t sysctlRescWdt1 = 0x00000020u; // Bit 5: Watchdog Timer 1

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

        application::Clocks::Initialize();
        peripherals.emplace();
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

    infra::MemoryRange<hal::GpioPin> PlatformFactoryImpl::Leds()
    {
        return infra::MakeRangeFromSingleObject(application::Pins::led1);
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
        auto samples = peripherals->motorFieldOrientedController.powerSupplyAdc.Measure(1);

        return foc::Volts{ static_cast<float>(samples.front()) * MotorFieldOrientedControllerInterfaceImpl::adcToVoltsFactor };
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
        peripherals->cortex.dataWatchPointAndTrace.Start();
        peripherals->performance.Set(true);
    }

    uint32_t PlatformFactoryImpl::ElapsedCycles()
    {
        peripherals->performance.Set(false);
        return peripherals->cortex.dataWatchPointAndTrace.Stop();
    }

    infra::CreatorBase<hal::SynchronousThreeChannelsPwm, void(std::chrono::nanoseconds deadTime, hal::Hertz frequency)>& PlatformFactoryImpl::SynchronousThreeChannelsPwmCreator()
    {
        return peripherals->motorFieldOrientedController.pwmBrushless;
    }

    infra::CreatorBase<AdcPhaseCurrentMeasurement, void(PlatformFactory::SampleAndHold)>& PlatformFactoryImpl::AdcMultiChannelCreator()
    {
        return peripherals->motorFieldOrientedController.adcCurrentPhases;
    }

    infra::CreatorBase<QuadratureEncoderDecorator, void()>& PlatformFactoryImpl::SynchronousQuadratureEncoderCreator()
    {
        return peripherals->encoderImpl.synchronousQuadratureEncoderCreator;
    }

    infra::CreatorBase<CanBusAdapter, void(uint32_t bitRate, bool testMode)>& PlatformFactoryImpl::CanBusCreator()
    {
        return peripherals->canImpl.canCreator;
    }

    hal::Eeprom& PlatformFactoryImpl::Eeprom()
    {
        return peripherals->eepromPeripheral;
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
