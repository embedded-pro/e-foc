#include "targets/platform_implementations/ti/implementation/PlatformFactoryImpl.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include "targets/platform_implementations/error_handling_cortex_m/PersistentFaultData.hpp"
#include DEVICE_HEADER

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

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
        return peripherals->performanceTrackerImpl;
    }

    hal::Hertz PlatformFactoryImpl::SystemClock() const
    {
        return hal::Hertz(SystemCoreClock);
    }

    foc::Volts PlatformFactoryImpl::PowerSupplyVoltage()
    {
        auto samples = peripherals->adcForPowerSupplyMeasurementImpl.powerSupplyAdc.Measure(1);

        return foc::Volts{ static_cast<float>(samples.front()) * AdcForPowerSupplyMeasurementImpl::adcToVoltsFactor };
    }

    foc::Ampere PlatformFactoryImpl::MaxCurrentSupported() const
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

    PlatformFactoryImpl::Peripherals::PerformanceTrackerImpl::PerformanceTrackerImpl(hal::DataWatchPointAndTrace& dwt, hal::OutputPin& pin)
        : dwt(dwt)
        , pin(pin)
    {}

    void PlatformFactoryImpl::Peripherals::PerformanceTrackerImpl::Start()
    {
        dwt.Start();
        pin.Set(true);
    }

    uint32_t PlatformFactoryImpl::Peripherals::PerformanceTrackerImpl::ElapsedCycles()
    {
        pin.Set(false);
        return dwt.Stop();
    }

    void PlatformFactoryImpl::ConfigureAdcAndPwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime, SampleAndHold sampleAndHold)
    {
        using namespace std::chrono_literals;
        auto& impl = peripherals->adcForPhaseCurrentMeasurementImpl;

        auto& adcCfg = impl.adcConfig;
        adcCfg.sampleAndHold = impl.toSampleAndHold.at(static_cast<std::size_t>(sampleAndHold));
        if constexpr (Peripheral::hasFaultComparators)
            adcCfg.digitalComparators = infra::MakeRange(impl.digitalComparators);

        peripherals->phaseCurrentAdc.reset();
        peripherals->phaseCurrentAdc.emplace(
            AdcForPhaseCurrentMeasurementImpl::adcToAmpereSlope,
            AdcForPhaseCurrentMeasurementImpl::adcToAmpereOffset,
            Peripheral::AdcIndex,
            Peripheral::AdcSequencerIndex,
            impl.currentPhaseAnalogPins,
            adcCfg);

        peripherals->asyncPwm.reset();
        peripherals->syncPwm.reset();
        if (Peripheral::hasFaultComparators)
        {
            auto& cfg = peripherals->asyncPwmConfig;
            cfg.deadTimeConfig.fallInClockCycles = hal::tiva::Pwm::CalculateDeadTimeCycles(deadTime, cfg.clockDivisor);
            cfg.deadTimeConfig.riseInClockCycles = hal::tiva::Pwm::CalculateDeadTimeCycles(deadTime, cfg.clockDivisor);
            cfg.pwmConfig.deadTime = std::make_optional(cfg.deadTimeConfig);

            peripherals->asyncPwm.emplace(
                Peripheral::PwmIndex,
                infra::MakeRange(Peripheral::asyncPwmPhases),
                cfg.pwmConfig,
                infra::Function<void(hal::tiva::Pwm::NormalEvent)>{},
                [this](hal::tiva::Pwm::FaultEvent ev)
                {
                    if (!onFaultCallback)
                        return;
                    const auto bits = static_cast<uint8_t>(ev.comparatorInputsByGenerator[1]);
                    if (bits & static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator0))
                        onFaultCallback(PlatformFactory::BoardProtectionReason::overCurrent);
                    if (bits & static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator1))
                        onFaultCallback(PlatformFactory::BoardProtectionReason::overVoltage);
                });
        }
        else
        {
            auto& cfg = peripherals->syncPwmConfig;
            cfg.deadTimeConfig.fallInClockCycles = hal::tiva::SynchronousPwm::CalculateDeadTimeCycles(deadTime, cfg.clockDivisor);
            cfg.deadTimeConfig.riseInClockCycles = hal::tiva::SynchronousPwm::CalculateDeadTimeCycles(deadTime, cfg.clockDivisor);
            cfg.pwmConfig.deadTime = std::make_optional(cfg.deadTimeConfig);

            peripherals->syncPwm.emplace(
                Peripheral::PwmIndex,
                infra::MakeRange(Peripheral::syncPwmPhases),
                cfg.pwmConfig);
        }
        if (Peripheral::hasFaultComparators)
            peripherals->asyncPwm->SetBaseFrequency(baseFrequency);
        else
            peripherals->syncPwm->SetBaseFrequency(baseFrequency);
        pwmBaseFrequency = baseFrequency;
    }

    void PlatformFactoryImpl::SetEncoderResolution(uint32_t resolution)
    {
        using Conf = hal::tiva::QuadratureEncoder::Config;
        hal::tiva::QuadratureEncoder::Config qeiConfig{ resolution, 0, false, false, false,
            Conf::ResetMode::onMaxPosition, Conf::CaptureMode::phaseAandPhaseB, Conf::SignalMode::quadrature };
        peripherals->encoder.reset();
        peripherals->encoder.emplace(resolution, Peripheral::QeiIndex, Pins::encoderA, Pins::encoderB, Pins::encoderZ, qeiConfig);
    }

    void PlatformFactoryImpl::ConfigureCanBus(uint32_t bitRate, bool testMode)
    {
        hal::tiva::Can::Config canConfig;
        canConfig.timing = hal::tiva::Can::BitRate{ bitRate };
        canConfig.testMode = testMode;

        peripherals->canBus.reset();
        peripherals->canBus.emplace(
            Peripheral::CanIndex,
            Pins::canRx,
            Pins::canTx,
            canConfig,
            infra::Function<void(hal::tiva::Can::Error)>([this](hal::tiva::Can::Error error)
                {
                    peripherals->canBus->InvokeErrorHandler(ToAdapterError(error));
                }));
    }

    CanBusAdapter& PlatformFactoryImpl::CanBus()
    {
        return *peripherals->canBus;
    }

    OPTIMIZE_FOR_SPEED void PlatformFactoryImpl::PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents)>& onDone)
    {
        onPhaseCurrentsReady = onDone;
        if (Peripheral::hasFaultComparators)
            peripherals->asyncPwm->SetBaseFrequency(baseFrequency);
        else
            peripherals->syncPwm->SetBaseFrequency(baseFrequency);
        peripherals->phaseCurrentAdc->Measure([this](foc::Ampere a, foc::Ampere b, foc::Ampere c)
            {
                onPhaseCurrentsReady(foc::PhaseCurrents{ a, b, c });
            });
    }

    OPTIMIZE_FOR_SPEED void PlatformFactoryImpl::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        if (Peripheral::hasFaultComparators)
            peripherals->asyncPwm->Start(dutyPhases.a, dutyPhases.b, dutyPhases.c);
        else
            peripherals->syncPwm->Start(dutyPhases.a, dutyPhases.b, dutyPhases.c);
    }

    void PlatformFactoryImpl::Start()
    {
        if (Peripheral::hasFaultComparators)
            peripherals->asyncPwm->Start(hal::Percent{ 1 }, hal::Percent{ 1 }, hal::Percent{ 1 });
        else
            peripherals->syncPwm->Start(hal::Percent{ 1 }, hal::Percent{ 1 }, hal::Percent{ 1 });
    }

    void PlatformFactoryImpl::Stop()
    {
        if (Peripheral::hasFaultComparators)
            peripherals->asyncPwm->Stop();
        else
            peripherals->syncPwm->Stop();
    }

    hal::Hertz PlatformFactoryImpl::BaseFrequency() const
    {
        return pwmBaseFrequency;
    }

    OPTIMIZE_FOR_SPEED foc::Radians PlatformFactoryImpl::Read()
    {
        return peripherals->encoder->Read() - encoderOffset;
    }

    void PlatformFactoryImpl::Set(foc::Radians value)
    {
        encoderOffset = peripherals->encoder->Read() - value;
    }

    void PlatformFactoryImpl::SetZero()
    {
        encoderOffset = peripherals->encoder->Read();
    }

    hal::Eeprom& PlatformFactoryImpl::Eeprom()
    {
        return peripherals->eepromPeripheral;
    }

    void PlatformFactoryImpl::RegisterBoardProtection(const infra::Function<void(PlatformFactory::BoardProtectionReason)>& onProtection)
    {
        onFaultCallback = onProtection;
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
