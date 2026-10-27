#pragma once

#include <optional>
#include <type_traits>
#include HARDWARE_PINS_AND_PERIPHERALS_HEADER
#include MOTOR_BOARD_CHARACTERISTICS_HEADER
#include "core/platform_abstraction/AdcPhaseCurrentMeasurement.hpp"
#include "core/platform_abstraction/CanBusAdapter.hpp"
#include "core/platform_abstraction/LowSideShuntWindow.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/platform_abstraction/QuadratureEncoderDecorator.hpp"
#include "core/services/cli/SerialCommunicationMirror.hpp"
#include "hal/cortex_m/DataWatchpointAndTrace.hpp"
#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/cortex_m/SystemTickTimerService.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal_tiva/synchronous_tiva/SynchronousAdc.hpp"
#include "hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal_tiva/synchronous_tiva/SynchronousQuadratureEncoder.hpp"
#include "hal_tiva/tiva/Adc.hpp"
#include "hal_tiva/tiva/Can.hpp"
#include "hal_tiva/tiva/Dma.hpp"
#include "hal_tiva/tiva/Eeprom.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "hal_tiva/tiva/Pwm.hpp"
#include "hal_tiva/tiva/UartWithDma.hpp"
#include "hal_tiva/tiva/WatchDog.hpp"
#include "infra/util/HotSwapFunction.hpp"
#include "services/tracer/SerialCommunicationOnSeggerRtt.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"
#include "services/util/EventDispatcherWatchdog.hpp"
#include "targets/platform_implementations/cortex_m_common/CycleCounter.hpp"
#include "targets/platform_implementations/cortex_m_common/FocLowPriorityInterruptAdapter.hpp"
#include "targets/platform_implementations/error_handling_cortex_m/PersistentFaultData.hpp"

extern "C" uint32_t SystemCoreClock;

namespace application
{
    using namespace std::chrono_literals;

    struct InterruptPriorities
    {
        static constexpr auto phaseCurrentAdc{ hal::cortex::InterruptPriority::highest };
        static constexpr auto pwmFault{ hal::cortex::InterruptPriority::highest };
        static constexpr auto can{ hal::cortex::InterruptPriority::normal };
        static constexpr auto uart{ hal::cortex::InterruptPriority::low };
        // No higher than any interrupt that could hang, so a hung handler starves the refresh and the hardware resets
        static constexpr auto watchdogEarlyWarning{ hal::cortex::InterruptPriority::lowest };
    };

    class PlatformFactoryImpl
        : public PlatformFactory
    {
    public:
        explicit PlatformFactoryImpl(const infra::Function<void()>& onInitialized);

        void ConfigureAdcAndPwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime, SampleAndHold sampleAndHold) override;
        drivers::DeadTimeLoss InverterDeadTimeLoss(std::chrono::nanoseconds deadTime) const override;
        uint32_t DutyUpdateDelayPeriods() const override;
        void SetEncoderResolution(uint32_t resolution) override;
        void ConfigureCanBus(uint32_t bitRate, bool testMode) override;
        CanBusAdapter& CanBus() override;

        void Run() override;
        services::Tracer& Tracer() override;
        services::TerminalWithCommands& Terminal() override;
        hal::GpioPin& OperationalLed() override;
        hal::GpioPin& WarningLed() override;
        hal::GpioPin& FailureLed() override;
        uint8_t BoardId() const override;
        bool PowerStatus() const override;
        hal::PerformanceTracker& PerformanceTimer() override;
        hal::Hertz SystemClock() const override;
        foc::Volts PowerSupplyVoltage() override;
        foc::LowPriorityInterrupt& LowPriorityInterrupt() override;
        hal::Eeprom& Eeprom() override;
        void RegisterBoardProtection(const infra::Function<void(PlatformFactory::BoardProtectionReason)>& onProtection) override;
        PlatformFactory::BoardProtectionState BoardProtectionStatus() override;
        void Reset() override;
        ResetCause GetResetCause() const override;
        infra::BoundedConstString FaultStatus() const override;
        PlatformDiagnostics& Diagnostics() override;

        void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

    private:
        static constexpr float adcReferenceVoltage = 3.3f;
        static constexpr float adcResolution = 4096.0f;

        // The UART hands the terminal a whole DMA half-buffer in one call from the ISR, and
        // QueueForOneReaderOneIrqWriter copies the range without checking that it fits.
        static_assert(Resources::terminalQueueSize >= Resources::uartReceiveBufferSize / 2,
            "terminal queue must absorb a whole DMA half-buffer");
        static_assert(Resources::faultStatusSize >= PersistentFaultData::kMaxFormattedSize,
            "fault status must hold the full register, FSR/FAR and backtrace dump");

        struct Cortex
        {
            static constexpr uint8_t watchdogIndex{ 0 };
            static constexpr infra::Duration watchdogEarlyWarningPeriod{ std::chrono::milliseconds(25) };
            static constexpr infra::Duration watchdogExpirationTimeout{ std::chrono::milliseconds(100) };

            static hal::tiva::WatchDog::Config WatchdogConfig();
            static void OnWatchdogExpired();

            hal::tiva::WatchDog watchdog{ watchdogIndex, WatchdogConfig() };
            services::EventDispatcherWithWeakPtrAndWatchdog::WithSize<Resources::eventDispatcherSize> eventDispatcher{ watchdog, watchdogExpirationTimeout, []()
                {
                    OnWatchdogExpired();
                } };
            hal::cortex::DataWatchpointAndTrace dataWatchPointAndTrace;
            hal::cortex::SystemTickTimerService systemTick{ SystemCoreClock, std::chrono::milliseconds(1) };
        };

        struct TerminalAndTracer
        {
            hal::tiva::Dma dma{ infra::emptyFunction };
            hal::tiva::UartWithDma::Config uartConfig{ true, true, hal::tiva::UartWithDma::Baudrate::_921600_bps, hal::tiva::UartWithDma::FlowControl::none, hal::tiva::UartWithDma::Parity::none, hal::tiva::UartWithDma::StopBits::one, hal::tiva::UartWithDma::NumberOfBytes::_8_bytes, std::make_optional(InterruptPriorities::uart) };
            hal::tiva::UartWithDma::WithRxBuffer<Resources::uartReceiveBufferSize> uart{ Peripheral::UartIndex, Pins::uartTx, Pins::uartRx, dma, uartConfig };
#ifdef E_FOC_CONSOLE_ON_RTT
            services::SerialCommunicationOnSeggerRtt rtt;
            services::SerialCommunicationMirror mirror{ uart, rtt };
            hal::SerialCommunication& console{ mirror };
#else
            hal::SerialCommunication& console{ uart };
#endif
            services::StreamWriterOnSerialCommunication::WithStorage<Resources::tracerBufferSize> streamWriterOnSerialCommunication{ console };
            infra::TextOutputStream::WithErrorPolicy tracerStream{ streamWriterOnSerialCommunication };
            services::TracerWithDateTime tracer{ tracerStream };
            services::TerminalWithCommandsImplBase<Resources::terminalCommandLength>::WithMaxQueueAndMaxHistory<Resources::terminalQueueSize, Resources::terminalHistorySize> terminal{ console, tracer };
        };

        struct AdcForPowerSupplyMeasurementImpl
        {
            static constexpr float adcToVoltsFactor = application::BoardCharacteristics::AdcToVoltsFactor(adcReferenceVoltage, adcResolution);
            infra::Function<void(std::tuple<infra::Ampere, infra::Ampere, infra::Ampere> voltagePhases)> phaseCurrentsReady;

            explicit AdcForPowerSupplyMeasurementImpl(hal::tiva::AnalogPin& powerSupplyPin)
                : powerSupplyAdc{ 1, 0, infra::MakeRangeFromSingleObject(powerSupplyPin), powerSupplyAdcConfig }
            {}

            constexpr static auto powerSupplyOversampling = hal::tiva::SynchronousAdc::Oversampling::oversampling8;
            hal::tiva::SynchronousAdc::Config powerSupplyAdcConfig{ hal::tiva::SynchronousAdc::SampleAndHold::sampleAndHold256, hal::tiva::SynchronousAdc::Priority::priority3, std::make_optional(powerSupplyOversampling) };
            hal::tiva::SynchronousAdc powerSupplyAdc;
        };

        struct AdcForPhaseCurrentMeasurementImpl
        {
            static constexpr std::array<hal::tiva::Adc::SampleAndHold, 5> toSampleAndHold{ { hal::tiva::Adc::SampleAndHold::sampleAndHold4,
                hal::tiva::Adc::SampleAndHold::sampleAndHold16,
                hal::tiva::Adc::SampleAndHold::sampleAndHold32,
                hal::tiva::Adc::SampleAndHold::sampleAndHold64,
                hal::tiva::Adc::SampleAndHold::sampleAndHold256 } };
            static constexpr float adcToAmpereSlope = application::BoardCharacteristics::AdcToAmpereSlope(adcReferenceVoltage, adcResolution);
            static constexpr float adcToAmpereOffset = application::BoardCharacteristics::AdcToAmpereOffset(adcReferenceVoltage);

            static constexpr hal::tiva::Adc::SamplingDelay phaseDelay{ 4 };
            static constexpr auto currentSensingOversampling = hal::tiva::Adc::Oversampling::oversampling2;

            // Steps 0-2 fill the ADC FIFO; steps 3-5 feed digital comparators 0-2 and trip PWM FLTSRC1.
            static constexpr std::array<hal::tiva::Adc::DigitalComparatorConfig, 6> digitalComparators{ {
                {},
                {},
                {},
                { Peripheral::OvercurrentComparatorIndex, 0, Peripheral::overcurrentThresholdCounts,
                    hal::tiva::Adc::ComparatorCondition::highBand, hal::tiva::Adc::ComparatorMode::always },
                { Peripheral::OvervoltageComparatorIndex, 0, Peripheral::overvoltageThresholdCounts,
                    hal::tiva::Adc::ComparatorCondition::highBand, hal::tiva::Adc::ComparatorMode::always },
                { Peripheral::UndervoltageComparatorIndex, Peripheral::undervoltageThresholdCounts, Peripheral::undervoltageThresholdCounts,
                    hal::tiva::Adc::ComparatorCondition::lowBand, hal::tiva::Adc::ComparatorMode::always },
            } };

            hal::tiva::Adc::Config adcConfig{ false, 0, Peripheral::adcTrigger, hal::tiva::Adc::SampleAndHold::sampleAndHold8, std::make_optional(currentSensingOversampling), phaseDelay, {}, hal::cortex::InterruptPriority::highest };
            static constexpr std::size_t powerSupplyPinIndex{ 4 };
            std::array<hal::tiva::AnalogPin, 6> currentPhaseAnalogPins{ { hal::tiva::AnalogPin{ Pins::currentPhaseA }, hal::tiva::AnalogPin{ Pins::currentPhaseB }, hal::tiva::AnalogPin{ Pins::currentPhaseC }, hal::tiva::AnalogPin{ Pins::currentTotal }, hal::tiva::AnalogPin{ Pins::powerSupplyVoltage }, hal::tiva::AnalogPin{ Pins::powerSupplyVoltageForUndervoltage } } };
        };

        struct AsyncPwmConfig
        {
            hal::tiva::Pwm::Config::ClockDivisor clockDivisor{ hal::tiva::Pwm::Config::ClockDivisor::divisor2 };
            hal::tiva::Pwm::Config::Control controlConfig{ hal::tiva::Pwm::Config::Control::Mode::centerAligned, hal::tiva::Pwm::Config::Control::UpdateMode::globally, false };
            hal::tiva::Pwm::Config::DeadTime deadTimeConfig{ hal::tiva::Pwm::CalculateDeadTimeCycles(1000ns, clockDivisor), hal::tiva::Pwm::CalculateDeadTimeCycles(1000ns, clockDivisor) };

            hal::tiva::Pwm::Config::InterruptConfig interruptConfig{
                {},
                infra::BoundedVector<hal::tiva::Pwm::Config::InterruptConfig::FaultConfig>::WithMaxSize<4>{ {
                    hal::tiva::Pwm::Config::InterruptConfig::FaultConfig{ hal::tiva::Pwm::GeneratorIndex::generator1, uint8_t{ 0x00 }, uint8_t{ static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator0) | static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator1) | static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator2) }, true, uint16_t{ 0 } },
                    hal::tiva::Pwm::Config::InterruptConfig::FaultConfig{ hal::tiva::Pwm::GeneratorIndex::generator2, uint8_t{ 0x00 }, uint8_t{ static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator0) | static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator1) | static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator2) }, true, uint16_t{ 0 } },
                    hal::tiva::Pwm::Config::InterruptConfig::FaultConfig{ hal::tiva::Pwm::GeneratorIndex::generator3, uint8_t{ 0x00 }, uint8_t{ static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator0) | static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator1) | static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator2) }, true, uint16_t{ 0 } },
                } },
                InterruptPriorities::pwmFault,
            };

            hal::tiva::Pwm::Config pwmConfig{ false, false, controlConfig, clockDivisor, std::make_optional(deadTimeConfig), std::make_optional(interruptConfig) };
        };

        struct SyncPwmConfig
        {
            hal::tiva::SynchronousPwm::Config::ClockDivisor clockDivisor{ hal::tiva::SynchronousPwm::Config::ClockDivisor::divisor2 };
            hal::tiva::SynchronousPwm::Config::Control controlConfig{ hal::tiva::SynchronousPwm::Config::Control::Mode::centerAligned, hal::tiva::SynchronousPwm::Config::Control::UpdateMode::globally, false };
            hal::tiva::SynchronousPwm::Config::DeadTime deadTimeConfig{ hal::tiva::SynchronousPwm::CalculateDeadTimeCycles(1000ns, clockDivisor), hal::tiva::SynchronousPwm::CalculateDeadTimeCycles(1000ns, clockDivisor) };
            hal::tiva::SynchronousPwm::Config pwmConfig{ false, false, controlConfig, clockDivisor, std::make_optional(deadTimeConfig) };
        };

        using PwmDriver = Peripheral::hal_pwm;
        using PwmConfiguration = std::conditional_t<Peripheral::hasFaultComparators, AsyncPwmConfig, SyncPwmConfig>;

        void ReconfigureAdc(SampleAndHold sampleAndHold);
        void ReconfigurePwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime);
        void OnPwmFault(hal::tiva::Pwm::FaultEvent event);
        void TripOverCurrent();

        // Condition must depend on Config: a discarded if constexpr branch is still type-checked otherwise.
        template<typename Config>
        void EmplacePwm(Config& config)
        {
            if constexpr (std::is_same_v<Config, AsyncPwmConfig>)
                peripherals->pwm.emplace(
                    Peripheral::PwmIndex,
                    infra::MakeRange(Peripheral::pwmPhases),
                    config.pwmConfig,
                    infra::Function<void(hal::tiva::Pwm::NormalEvent)>{},
                    [this](hal::tiva::Pwm::FaultEvent event)
                    {
                        OnPwmFault(event);
                    });
            else
                peripherals->pwm.emplace(
                    Peripheral::PwmIndex,
                    infra::MakeRange(Peripheral::pwmPhases),
                    config.pwmConfig);
        }

        static CanBusAdapter::CanError ToAdapterError(hal::tiva::Can::Error error)
        {
            switch (error)
            {
                case hal::tiva::Can::Error::busOff:
                    return CanBusAdapter::CanError::busOff;
                case hal::tiva::Can::Error::errorPassive:
                    return CanBusAdapter::CanError::errorPassive;
                case hal::tiva::Can::Error::errorWarning:
                    return CanBusAdapter::CanError::errorWarning;
                case hal::tiva::Can::Error::messageLost:
                    return CanBusAdapter::CanError::messageLost;
                case hal::tiva::Can::Error::ackError:
                    return CanBusAdapter::CanError::ackError;
                case hal::tiva::Can::Error::stuffError:
                    return CanBusAdapter::CanError::stuffError;
                case hal::tiva::Can::Error::formError:
                    return CanBusAdapter::CanError::formError;
                case hal::tiva::Can::Error::crcError:
                    return CanBusAdapter::CanError::crcError;
                case hal::tiva::Can::Error::bit0Error:
                    return CanBusAdapter::CanError::bit0Error;
                case hal::tiva::Can::Error::bit1Error:
                    return CanBusAdapter::CanError::bit1Error;
                default:
                    return CanBusAdapter::CanError::other;
            }
        }

        struct Peripherals
        {
            Peripherals() = default;

            hal::OutputPin performance{ Pins::performance };
            Cortex cortex;
            TerminalAndTracer terminalAndTracer;
            AdcForPhaseCurrentMeasurementImpl adcForPhaseCurrentMeasurementImpl;
            AdcForPowerSupplyMeasurementImpl adcForPowerSupplyMeasurementImpl{ adcForPhaseCurrentMeasurementImpl.currentPhaseAnalogPins[AdcForPhaseCurrentMeasurementImpl::powerSupplyPinIndex] };
            PwmConfiguration pwmConfig;
            hal::tiva::Eeprom eepromPeripheral;

            std::optional<AdcPhaseCurrentMeasurementImpl<hal::tiva::Adc>> phaseCurrentAdc;
            std::optional<PwmDriver> pwm;
            std::optional<QuadratureEncoderDecoratorImpl<hal::tiva::QuadratureEncoder>> encoder;
            std::optional<CanBusAdapterImpl<hal::tiva::Can::WithMaxRxBuffer<Resources::canReceiveBufferSize>>> canBus;

            struct PerformanceTrackerImpl
                : hal::PerformanceTracker
            {
                explicit PerformanceTrackerImpl(hal::cortex::DataWatchpointAndTrace& dwt, hal::OutputPin& pin);
                void Start() override;
                uint32_t ElapsedCycles() override;

                hal::cortex::DataWatchpointAndTrace& dwt;
                hal::OutputPin& pin;
            } performanceTrackerImpl{ cortex.dataWatchPointAndTrace, performance };
        };

        template<typename Fn>
        void WithPwm(Fn&& fn)
        {
            fn(*peripherals->pwm);
        }

    private:
        static void ConfigureBoardIdPins();
        static uint8_t ReadBoardIdPins();

        infra::Function<void()> onInitialized;
        FocLowPriorityInterruptAdapter pendSvLowPriorityInterrupt;
        ResetCause resetCause{ ResetCause::powerUp };
        uint8_t boardId{ 0 };
        [[no_unique_address]] CycleCounter cycleCounter;
        ControlLoopMetrics controlLoopMetrics;
        PlatformDiagnostics diagnostics{ controlLoopMetrics };
        volatile bool controlLoopEntered{ false };
        volatile bool overCurrentTripped{ false };
        foc::LowSideShuntReadout shuntReadout{ Peripheral::maxReadableDuty };
        infra::BoundedString::WithStorage<Resources::faultStatusSize> faultStatusString;
        hal::Hertz pwmBaseFrequency{ 20000 };
        foc::Radians encoderOffset{ 0.0f };
        infra::HotSwapFunction<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
        infra::Function<void(BoardProtectionReason)> onFaultCallback;
        std::optional<Peripherals> peripherals;
    };
}
