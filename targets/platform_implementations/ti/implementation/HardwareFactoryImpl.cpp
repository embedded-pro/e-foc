#include "targets/platform_implementations/ti/implementation/HardwareFactoryImpl.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include "source/platform_abstraction/HardwareFactory.hpp"
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

    HardwareFactoryImpl::HardwareFactoryImpl(const infra::Function<void()>& onInitialized)
        : onInitialized(onInitialized)
    {
        application::Clocks::Initialize();
        peripherals.emplace();
        services::SetGlobalTracerInstance(peripherals->terminalAndTracer.tracer);
        this->onInitialized();
    }

    void HardwareFactoryImpl::Run()
    {
        peripherals->cortex.eventDispatcher.Run();
    }

    services::Tracer& HardwareFactoryImpl::Tracer()
    {
        return peripherals->terminalAndTracer.tracer;
    }

    services::TerminalWithCommands& HardwareFactoryImpl::Terminal()
    {
        return peripherals->terminalAndTracer.terminal;
    }

    infra::MemoryRange<hal::GpioPin> HardwareFactoryImpl::Leds()
    {
        return infra::MakeRangeFromSingleObject(application::Pins::led1);
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
        auto samples = peripherals->motorFieldOrientedController.powerSupplyAdc.Measure(1);

        return foc::Volts{ static_cast<float>(samples.front()) * MotorFieldOrientedControllerInterfaceImpl::adcToVoltsFactor };
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
        peripherals->cortex.dataWatchPointAndTrace.Start();
        peripherals->performance.Set(true);
    }

    uint32_t HardwareFactoryImpl::ElapsedCycles()
    {
        peripherals->performance.Set(false);
        return peripherals->cortex.dataWatchPointAndTrace.Stop();
    }

    infra::CreatorBase<hal::SynchronousThreeChannelsPwm, void(std::chrono::nanoseconds deadTime, hal::Hertz frequency)>& HardwareFactoryImpl::SynchronousThreeChannelsPwmCreator()
    {
        return peripherals->motorFieldOrientedController.pwmBrushless;
    }

    infra::CreatorBase<AdcPhaseCurrentMeasurement, void(HardwareFactory::SampleAndHold)>& HardwareFactoryImpl::AdcMultiChannelCreator()
    {
        return peripherals->motorFieldOrientedController.adcCurrentPhases;
    }

    infra::CreatorBase<QuadratureEncoderDecorator, void()>& HardwareFactoryImpl::SynchronousQuadratureEncoderCreator()
    {
        return peripherals->encoderImpl.synchronousQuadratureEncoderCreator;
    }

    infra::CreatorBase<CanBusAdapter, void(uint32_t bitRate, bool testMode)>& HardwareFactoryImpl::CanBusCreator()
    {
        return peripherals->canImpl.canCreator;
    }
}
