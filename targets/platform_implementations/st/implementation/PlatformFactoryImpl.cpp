#include "targets/platform_implementations/st/implementation/PlatformFactoryImpl.hpp"
#include "generated/stm32fxxx/PinoutTableDefault.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include "targets/platform_implementations/error_handling_cortex_m/CutPowerStage.hpp"
#include "targets/platform_implementations/error_handling_cortex_m/PersistentFaultData.hpp"
#include DEVICE_HEADER
#include "stm32g4xx_ll_tim.h"
#include <cmath>

namespace
{
    application::ResetCause ReadAndClearResetCause()
    {
        const uint32_t csr = RCC->CSR;
        RCC->CSR |= RCC_CSR_RMVF;

        if (csr & RCC_CSR_SFTRSTF)
            return application::ResetCause::software;
        if (csr & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF))
            return application::ResetCause::watchdog;
        if (csr & RCC_CSR_BORRSTF)
            return application::ResetCause::powerUp;
        if (csr & RCC_CSR_PINRSTF)
            return application::ResetCause::hardware;
        return application::ResetCause::powerUp;
    }
}

namespace application
{
    namespace
    {
        constexpr std::array<uint32_t, 5> toSamplingTime{ {
            ADC_SAMPLETIME_12CYCLES_5,
            ADC_SAMPLETIME_47CYCLES_5,
            ADC_SAMPLETIME_92CYCLES_5,
            ADC_SAMPLETIME_247CYCLES_5,
            ADC_SAMPLETIME_640CYCLES_5,
        } };

        constexpr float adcToAmpereSlope = BoardCharacteristics::AdcToAmpereSlope(Peripheral::adcReferenceVoltage, Peripheral::adcResolution);
        constexpr float adcToAmpereOffset = BoardCharacteristics::AdcToAmpereOffset(Peripheral::adcReferenceVoltage);
        constexpr float adcToVoltsFactor = BoardCharacteristics::AdcToVoltsFactor(Peripheral::adcReferenceVoltage, Peripheral::adcResolution);

        constexpr uint32_t eepromI2cTiming{ 0xf0d42c35 };

        bool ExceedsOverCurrent(foc::Ampere a, foc::Ampere b, foc::Ampere c)
        {
            constexpr float limit{ BoardCharacteristics::overcurrentThresholdAmps };

            return std::abs(a.Value()) > limit || std::abs(b.Value()) > limit || std::abs(c.Value()) > limit;
        }

        hal::SynchronousAdcStm::Config PowerSupplyAdcConfig()
        {
            hal::SynchronousAdcStm::Config config;
            config.samplingTime = ADC_SAMPLETIME_247CYCLES_5;
            config.oversampling = hal::SynchronousAdcStm::Oversampling::x8;
            return config;
        }

        hal::I2cStm::Config EepromI2cConfig()
        {
            hal::I2cStm::Config config;
            config.timing = eepromI2cTiming;
            return config;
        }

        hal::UartStmDma::Config ConsoleConfig()
        {
            hal::UartStmDma::Config config;
            config.baudrate = 921600;
            config.priority = InterruptPriorities::uart;
            return config;
        }
    }

    PlatformFactoryImpl::HalInitialization::HalInitialization()
    {
        HAL_Init();
        Clocks::Initialize();
    }

    PlatformFactoryImpl::TerminalAndTracer::TerminalAndTracer(hal::DmaStm& dma)
        : transmitStream{ dma, hal::DmaChannelId{ Peripheral::DmaIndex, Peripheral::UartTxDmaChannel, Peripheral::uartTxDmaRequest } }
        , uart{ transmitStream, Peripheral::LpUartIndex, Pins::uartTx, Pins::uartRx, hal::lpUart, ConsoleConfig() }
    {}

    PlatformFactoryImpl::Cortex::Cortex(PlatformFactoryImpl& platform)
        : eventDispatcher{ watchdog, watchdogExpirationTimeout, [&platform]()
            {
                platform.OnWatchdogExpired();
            } }
    {}

    PlatformFactoryImpl::Peripherals::Peripherals(PlatformFactoryImpl& platform)
        : cortex{ platform }
        , powerSupplyAdc{ powerSupplyConverter, Peripheral::powerSupplyAdcChannel, PowerSupplyAdcConfig() }
        , eepromI2c{ Peripheral::EepromI2cIndex, Pins::eepromScl, Pins::eepromSda, EepromI2cConfig() }
    {}

    PlatformFactoryImpl::PlatformFactoryImpl(const infra::Function<void()>& onInitialized)
        : gpio(hal::pinoutTableDefaultStm, hal::analogTableDefaultStm)
        , onInitialized(onInitialized)
        , resetCause(ReadAndClearResetCause())
    {
        if (persistentFaultData.IsValid())
        {
            FormatFaultData(persistentFaultData, faultStatusString);
            persistentFaultData.Invalidate();
        }

        ConfigureBoardIdPins();
        peripherals.emplace(*this);
        boardId = ReadBoardIdPins();

        NVIC_SetPriority(WWDG_IRQn, (1u << __NVIC_PRIO_BITS) - 1u);

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

    hal::GpioPin& PlatformFactoryImpl::OperationalLed()
    {
        return Pins::operationalLed;
    }

    hal::GpioPin& PlatformFactoryImpl::WarningLed()
    {
        return Pins::warningLed;
    }

    hal::GpioPin& PlatformFactoryImpl::FailureLed()
    {
        return Pins::failureLed;
    }

    uint8_t PlatformFactoryImpl::BoardId() const
    {
        return boardId;
    }

    void PlatformFactoryImpl::ConfigureBoardIdPins()
    {
        if constexpr (Peripheral::hasBoardIdPins)
        {
            Pins::boardId0.Config(hal::PinConfigType::input);
            Pins::boardId1.Config(hal::PinConfigType::input);
            Pins::boardId2.Config(hal::PinConfigType::input);
        }
    }

    uint8_t PlatformFactoryImpl::ReadBoardIdPins()
    {
        if constexpr (!Peripheral::hasBoardIdPins)
            return 0;

        const uint8_t bit0 = Pins::boardId0.Get() ? 0u : 1u;
        const uint8_t bit1 = Pins::boardId1.Get() ? 0u : 1u;
        const uint8_t bit2 = Pins::boardId2.Get() ? 0u : 1u;
        return static_cast<uint8_t>(bit0 | (bit1 << 1u) | (bit2 << 2u));
    }

    hal::Hertz PlatformFactoryImpl::SystemClock() const
    {
        return hal::Hertz(SystemCoreClock);
    }

    foc::Volts PlatformFactoryImpl::PowerSupplyVoltage()
    {
        return foc::Volts{ static_cast<float>(peripherals->powerSupplyAdc.Measure(1).front()) * adcToVoltsFactor };
    }

    foc::Ampere PlatformFactoryImpl::MaxCurrentSupported() const
    {
        return foc::Ampere(BoardCharacteristics::maxCurrentSupportedAmps);
    }

    drivers::DeadTimeLoss PlatformFactoryImpl::InverterDeadTimeLoss(std::chrono::nanoseconds deadTime) const
    {
        if (deadTime != std::chrono::nanoseconds{ BoardCharacteristics::measuredDeadBandNanoseconds })
            return {};

        return { std::chrono::nanoseconds{ BoardCharacteristics::effectiveDeadTimeNanoseconds }, foc::Ampere{ BoardCharacteristics::deadTimeKneeAmps } };
    }

    uint32_t PlatformFactoryImpl::DutyUpdateDelayPeriods() const
    {
        return 1;
    }

    foc::LowPriorityInterrupt& PlatformFactoryImpl::LowPriorityInterrupt()
    {
        return pendSvLowPriorityInterrupt;
    }

    hal::WatchDogStm::Config PlatformFactoryImpl::Cortex::WatchdogConfig()
    {
        hal::WatchDogStm::Config config;
        config.prescaler = WWDG_PRESCALER_8;
        return config;
    }

    void PlatformFactoryImpl::OnWatchdogExpired()
    {
        protectionTripped = true;
        CutPowerStage();
    }

    void PlatformFactoryImpl::ConfigureAdcAndPwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime, SampleAndHold sampleAndHold)
    {
        const auto periodCycles = baseFrequency.Value() == 0
                                      ? 0u
                                      : static_cast<uint32_t>(SystemCoreClock / baseFrequency.Value());
        controlLoopMetrics.Configure(static_cast<uint32_t>(static_cast<uint64_t>(periodCycles) * 3u / 4u), periodCycles);

        ReconfigureAdc(sampleAndHold);
        ReconfigurePwm(baseFrequency, deadTime);
        pwmBaseFrequency = baseFrequency;
    }

    void PlatformFactoryImpl::ReconfigureAdc(SampleAndHold sampleAndHold)
    {
        const auto samplingTime = toSamplingTime.at(static_cast<std::size_t>(sampleAndHold));

        if (peripherals->phaseCurrentAdc)
            peripherals->phaseCurrentAdc->Stop();

        peripherals->phaseCurrentAdc.reset();
        peripherals->phaseCurrentAdc.emplace(
            adcToAmpereSlope,
            adcToAmpereOffset,
            peripherals->phaseCurrentConverter,
            peripherals->phaseCurrentStream,
            Peripheral::phaseCurrentAdcTrigger,
            [this](hal::AdcMultiChannel::Samples samples)
            {
                MonitorBoard(samples);
            },
            hal::AdcPinConfig{ Pins::currentPhaseA, samplingTime },
            hal::AdcPinConfig{ Pins::currentPhaseB, samplingTime },
            hal::AdcPinConfig{ Pins::currentPhaseC, samplingTime },
            hal::AdcPinConfig{ Pins::currentTotal, samplingTime },
            hal::AdcPinConfig{ Pins::powerSupplyVoltage, samplingTime });

        auto& dmaHandler = *hal::cortex::InterruptTable::Instance().Handler(Peripheral::phaseCurrentDmaIrq);
        dmaHandler.Unregister();
        dmaHandler.Register(Peripheral::phaseCurrentDmaIrq, InterruptPriorities::phaseCurrentAdc);
    }

    void PlatformFactoryImpl::ReconfigurePwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime)
    {
        peripherals->pwm.reset();

        hal::PwmStm::Config config;
        config.alignment = hal::PwmStm::Alignment::centerAlignedBothCounting;
        config.triggerOutput = hal::PwmStm::TriggerOutput::compareChannel4;
        config.deadTime.emplace();
        config.deadTime->duration = deadTime;

        peripherals->pwm.emplace(Peripheral::PwmTimerIndex, infra::MakeRange(Peripheral::pwmPhases), config);
        peripherals->pwm->SetBaseFrequency(baseFrequency);

        ConfigureSamplingTrigger();
    }

    void PlatformFactoryImpl::ConfigureSamplingTrigger()
    {
        constexpr uint32_t triggerLeadTicks{ 8 };
        auto timer = hal::peripheralTimer[Peripheral::PwmTimerIndex - 1];

        LL_TIM_OC_SetCompareCH4(timer, LL_TIM_GetAutoReload(timer) - triggerLeadTicks);
        LL_TIM_OC_SetMode(timer, LL_TIM_CHANNEL_CH4, LL_TIM_OCMODE_PWM2);
        LL_TIM_SetRepetitionCounter(timer, 1);
    }

    void PlatformFactoryImpl::MonitorBoard(hal::AdcMultiChannel::Samples samples)
    {
        const auto currentTotal = samples[currentTotalSample];
        const auto powerSupply = samples[powerSupplySample];

        if (currentTotal > Peripheral::overcurrentThresholdCounts)
            TripProtection(BoardProtectionReason::overCurrent);
        else if (powerSupply > Peripheral::overvoltageThresholdCounts)
            TripProtection(BoardProtectionReason::overVoltage);
        else if (powerSupply < Peripheral::undervoltageThresholdCounts)
            TripProtection(BoardProtectionReason::underVoltage);
    }

    void PlatformFactoryImpl::TripProtection(BoardProtectionReason reason)
    {
        CutPowerStage();

        if (protectionTripped)
            return;

        protectionTripped = true;
        infra::EventDispatcher::Instance().Schedule([this, reason]()
            {
                if (onFaultCallback)
                    onFaultCallback(reason);
            });
    }

    void PlatformFactoryImpl::SetEncoderResolution(uint32_t resolution)
    {
        hal::SynchronousQuadratureEncoderStm::Config config;
        config.resolution = resolution;
        config.invertPhaseA = Peripheral::encoderInvertPhaseA;
        config.filter = 3;

        peripherals->encoder.reset();
        peripherals->encoder.emplace(resolution, Peripheral::EncoderTimerIndex, Pins::encoderA, Pins::encoderB, Pins::encoderZ, config);
    }

    CanBusAdapter::CanError PlatformFactoryImpl::ToAdapterError(hal::FdCanStm::Error error)
    {
        switch (error)
        {
            case hal::FdCanStm::Error::busOff:
                return CanBusAdapter::CanError::busOff;
            case hal::FdCanStm::Error::errorPassive:
                return CanBusAdapter::CanError::errorPassive;
            case hal::FdCanStm::Error::errorWarning:
                return CanBusAdapter::CanError::errorWarning;
            case hal::FdCanStm::Error::messageLost:
                return CanBusAdapter::CanError::messageLost;
            case hal::FdCanStm::Error::ackError:
                return CanBusAdapter::CanError::ackError;
            case hal::FdCanStm::Error::stuffError:
                return CanBusAdapter::CanError::stuffError;
            case hal::FdCanStm::Error::formError:
                return CanBusAdapter::CanError::formError;
            case hal::FdCanStm::Error::crcError:
                return CanBusAdapter::CanError::crcError;
            case hal::FdCanStm::Error::bit0Error:
                return CanBusAdapter::CanError::bit0Error;
            case hal::FdCanStm::Error::bit1Error:
                return CanBusAdapter::CanError::bit1Error;
            default:
                return CanBusAdapter::CanError::other;
        }
    }

    void PlatformFactoryImpl::ConfigureCanBus(uint32_t bitRate, bool testMode)
    {
        hal::FdCanStm::Config canConfig;
        canConfig.timing = hal::FdCanStm::BitRate{ bitRate };
        canConfig.testMode = testMode;
        canConfig.interruptPriority = InterruptPriorities::can;

        peripherals->canBus.reset();
        peripherals->canBus.emplace(
            Peripheral::CanIndex,
            Pins::canRx,
            Pins::canTx,
            canConfig,
            infra::Function<void(hal::FdCanStm::Error)>([this](hal::FdCanStm::Error error)
                {
                    peripherals->canBus->InvokeErrorHandler(ToAdapterError(error));
                }));

        diagnostics.AttachCanBus(*peripherals->canBus);
    }

    CanBusAdapter& PlatformFactoryImpl::CanBus()
    {
        return *peripherals->canBus;
    }

    void PlatformFactoryImpl::PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents)>& onDone)
    {
        onPhaseCurrentsReady.Set(onDone);
        peripherals->phaseCurrentAdc->Measure([this](foc::Ampere a, foc::Ampere b, foc::Ampere c)
            {
                if (controlLoopEntered)
                {
                    controlLoopMetrics.RecordReentry();
                    return;
                }

                controlLoopEntered = true;
                const auto entryCycles = CycleCounter::Now();

                const auto currents = shuntReadout.Read(foc::PhaseCurrents{ a, b, c });

                if (protectionTripped)
                    CutPowerStage();
                else if (ExceedsOverCurrent(currents.a, currents.b, currents.c))
                    TripProtection(BoardProtectionReason::overCurrent);
                else
                    onPhaseCurrentsReady(currents);

                controlLoopMetrics.Record(CycleCounter::Now() - entryCycles);
                controlLoopEntered = false;
            });
    }

    void PlatformFactoryImpl::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        if (protectionTripped)
            return;

        const auto readable = foc::KeepLowSideShuntsReadable(dutyPhases, Peripheral::maxReadableDuty, Peripheral::maxDuty);
        shuntReadout.Applied(readable);

        peripherals->pwm->Start(readable.a, readable.b, readable.c);

        if (protectionTripped)
            CutPowerStage();
    }

    void PlatformFactoryImpl::Start()
    {
        if (protectionTripped)
            return;

        peripherals->pwm->Start(hal::DutyCycle::FromPercent(1), hal::DutyCycle::FromPercent(1), hal::DutyCycle::FromPercent(1));

        if (protectionTripped)
            CutPowerStage();
    }

    void PlatformFactoryImpl::Stop()
    {
        peripherals->pwm->Stop();
        protectionTripped = false;
    }

    hal::Hertz PlatformFactoryImpl::BaseFrequency() const
    {
        return pwmBaseFrequency;
    }

    foc::Radians PlatformFactoryImpl::Read()
    {
        if (!peripherals->encoder)
            return foc::Radians{ 0.0f };
        return peripherals->encoder->Read() - encoderOffset;
    }

    void PlatformFactoryImpl::Set(foc::Radians value)
    {
        if (peripherals->encoder)
            encoderOffset = peripherals->encoder->Read() - value;
    }

    void PlatformFactoryImpl::SetZero()
    {
        if (peripherals->encoder)
            encoderOffset = peripherals->encoder->Read();
    }

    hal::Eeprom& PlatformFactoryImpl::Eeprom()
    {
        return peripherals->eeprom;
    }

    void PlatformFactoryImpl::RegisterBoardProtection(const infra::Function<void(PlatformFactory::BoardProtectionReason)>& onProtection)
    {
        onFaultCallback = onProtection;
    }

    PlatformFactory::BoardProtectionState PlatformFactoryImpl::BoardProtectionStatus()
    {
        return PlatformFactory::BoardProtectionState::unknown;
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

    PlatformDiagnostics& PlatformFactoryImpl::Diagnostics()
    {
        return diagnostics;
    }
}
