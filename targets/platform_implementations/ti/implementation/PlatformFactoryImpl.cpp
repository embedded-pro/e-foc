#include "targets/platform_implementations/ti/implementation/PlatformFactoryImpl.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
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

namespace application
{
    extern "C" uint32_t SystemCoreClock;

    PlatformFactoryImpl::PlatformFactoryImpl(const infra::Function<void()>& onInitialized)
        : onInitialized(onInitialized)
    {
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
}
