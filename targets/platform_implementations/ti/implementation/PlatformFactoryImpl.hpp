#pragma once

#include <optional>
#include HARDWARE_PINS_AND_PERIPHERALS_HEADER
#include "core/platform_abstraction/AdcPhaseCurrentMeasurement.hpp"
#include "core/platform_abstraction/CanBusAdapter.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/platform_abstraction/QuadratureEncoderDecorator.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal_tiva/cortex/DataWatchpointAndTrace.hpp"
#include "hal_tiva/cortex/SystemTickTimerService.hpp"
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
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"

namespace application
{
    using namespace std::chrono_literals;

    class PlatformFactoryImpl
        : public PlatformFactory
    {
    public:
        explicit PlatformFactoryImpl(const infra::Function<void()>& onInitialized);

        // Implementation of PlatformFactory — configuration
        void ConfigureAdcAndPwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime, SampleAndHold sampleAndHold) override;
        void SetEncoderResolution(uint32_t resolution) override;
        void ConfigureCanBus(uint32_t bitRate, bool testMode) override;
        CanBusAdapter& CanBus() override;

        // Implementation of PlatformFactory — accessors
        void Run() override;
        services::Tracer& Tracer() override;
        services::TerminalWithCommands& Terminal() override;
        infra::MemoryRange<hal::GpioPin> Leds() override;
        hal::PerformanceTracker& PerformanceTimer() override;
        hal::Hertz SystemClock() const override;
        foc::Volts PowerSupplyVoltage() override;
        foc::LowPriorityInterrupt& LowPriorityInterrupt() override;
        hal::Eeprom& Eeprom() override;
        void RegisterBoardProtection(const infra::Function<void(PlatformFactory::BoardProtectionReason)>& onProtection) override;
        void Reset() override;
        ResetCause GetResetCause() const override;
        infra::BoundedConstString FaultStatus() const override;

        // Implementation of foc::ThreePhaseInverter
        OPTIMIZE_FOR_SPEED void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) override;
        OPTIMIZE_FOR_SPEED void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

        // Implementation of foc::Encoder
        OPTIMIZE_FOR_SPEED foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

    private:
        static constexpr float adcReferenceVoltage = 3.3f;
        static constexpr float adcResolution = 4096.0f;

        struct PendSvLowPriorityInterrupt
            : public foc::LowPriorityInterrupt
        {
            void Trigger() override;
            void Register(const infra::Function<void()>& handler) override;

            infra::Function<void()> onLowPriorityInterrupt;
        };

        struct Cortex
        {
            infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;
            hal::DataWatchPointAndTrace dataWatchPointAndTrace;
            hal::cortex::SystemTickTimerService systemTick{ std::chrono::milliseconds(1) };
        };

        struct TerminalAndTracer
        {
            hal::tiva::Dma dma{ infra::emptyFunction };
            hal::tiva::UartWithDma::Config uartConfig{ true, true, hal::tiva::UartWithDma::Baudrate::_921000_bps, hal::tiva::UartWithDma::FlowControl::none, hal::tiva::UartWithDma::Parity::none, hal::tiva::UartWithDma::StopBits::one, hal::tiva::UartWithDma::NumberOfBytes::_8_bytes, std::nullopt };
            hal::tiva::UartWithDma::WithRxBuffer<256> uart{ Peripheral::UartIndex, Pins::uartTx, Pins::uartRx, dma, uartConfig };
            services::StreamWriterOnSerialCommunication::WithStorage<8192> streamWriterOnSerialCommunication{ uart };
            infra::TextOutputStream::WithErrorPolicy tracerStream{ streamWriterOnSerialCommunication };
            services::TracerWithDateTime tracer{ tracerStream };
            services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<256, 10> terminal{ uart, tracer };
        };

        struct AdcForPowerSupplyMeasurementImpl
        {
            static constexpr float adcToVoltsFactor = application::BoardCharacteristics::AdcToVoltsFactor(adcReferenceVoltage, adcResolution);
            infra::Function<void(std::tuple<infra::Ampere, infra::Ampere, infra::Ampere> voltagePhases)> phaseCurrentsReady;
            std::array<hal::tiva::AnalogPin, 1> powerSupplyAnalogPins{ { hal::tiva::AnalogPin{ Pins::powerSupplyVoltage } } };
            constexpr static auto powerSupplyOversampling = hal::tiva::SynchronousAdc::Oversampling::oversampling8;
            hal::tiva::SynchronousAdc::Config powerSupplyAdcConfig{ hal::tiva::SynchronousAdc::SampleAndHold::sampleAndHold256, hal::tiva::SynchronousAdc::Priority::priority3, std::make_optional(powerSupplyOversampling) };
            hal::tiva::SynchronousAdc powerSupplyAdc{ 1, 0, powerSupplyAnalogPins, powerSupplyAdcConfig };
        };

        struct AdcForPhaseCurrentMeasurementImpl
        {
            const std::array<hal::tiva::Adc::SampleAndHold, 5> toSampleAndHold{ { hal::tiva::Adc::SampleAndHold::sampleAndHold4,
                hal::tiva::Adc::SampleAndHold::sampleAndHold16,
                hal::tiva::Adc::SampleAndHold::sampleAndHold32,
                hal::tiva::Adc::SampleAndHold::sampleAndHold64,
                hal::tiva::Adc::SampleAndHold::sampleAndHold256 } };
            static constexpr float adcToAmpereSlope = application::BoardCharacteristics::AdcToAmpereSlope(adcReferenceVoltage, adcResolution);
            static constexpr float adcToAmpereOffset = application::BoardCharacteristics::AdcToAmpereOffset(adcReferenceVoltage);

            static constexpr hal::tiva::Adc::SamplingDelay phaseDelay{ 4 };
            static constexpr auto currentSensingOversampling = hal::tiva::Adc::Oversampling::oversampling2;

            // Steps 0–2 go to the ADC FIFO (phase currents A/B/C).
            // Steps 3–4 are redirected to DCMP units 0 and 1 via the SSOP register and
            // do NOT appear in the FIFO, so AdcPhaseCurrentMeasurementImpl still receives
            // exactly 3 samples.  DCMP0/1 outputs connect to PWM FLTSRC1 bits 0/1 and
            // tristate all motor PWM outputs instantly when a threshold is exceeded.
            static constexpr std::array<hal::tiva::Adc::DigitalComparatorConfig, 5> digitalComparators{ {
                {}, // step 0: currentPhaseA  → FIFO (noComparator)
                {}, // step 1: currentPhaseB  → FIFO (noComparator)
                {}, // step 2: currentPhaseC  → FIFO (noComparator)
                { Peripheral::OvercurrentComparatorIndex, 0, Peripheral::overcurrentThresholdCounts,
                    hal::tiva::Adc::ComparatorCondition::highBand, hal::tiva::Adc::ComparatorMode::always },
                { Peripheral::OvervoltageComparatorIndex, 0, Peripheral::overvoltageThresholdCounts,
                    hal::tiva::Adc::ComparatorCondition::highBand, hal::tiva::Adc::ComparatorMode::always },
            } };

            hal::tiva::Adc::Config adcConfig{ false, 0, Peripheral::adcTrigger, hal::tiva::Adc::SampleAndHold::sampleAndHold8, std::make_optional(currentSensingOversampling), phaseDelay };
            std::array<hal::tiva::AnalogPin, 5> currentPhaseAnalogPins{ { hal::tiva::AnalogPin{ Pins::currentPhaseA }, hal::tiva::AnalogPin{ Pins::currentPhaseB }, hal::tiva::AnalogPin{ Pins::currentPhaseC }, hal::tiva::AnalogPin{ Pins::currentTotal }, hal::tiva::AnalogPin{ Pins::powerSupplyVoltage } } };
        };

        struct AsyncPwmConfig
        {
            hal::tiva::Pwm::Config::ClockDivisor clockDivisor{ hal::tiva::Pwm::Config::ClockDivisor::divisor8 };
            hal::tiva::Pwm::Config::Control controlConfig{ hal::tiva::Pwm::Config::Control::Mode::centerAligned, hal::tiva::Pwm::Config::Control::UpdateMode::globally, false };
            hal::tiva::Pwm::Config::DeadTime deadTimeConfig{ hal::tiva::Pwm::CalculateDeadTimeCycles(1000ns, clockDivisor), hal::tiva::Pwm::CalculateDeadTimeCycles(1000ns, clockDivisor) };

            hal::tiva::Pwm::Config::InterruptConfig interruptConfig{
                {},
                infra::BoundedVector<hal::tiva::Pwm::Config::InterruptConfig::FaultConfig>::WithMaxSize<4>{ {
                    hal::tiva::Pwm::Config::InterruptConfig::FaultConfig{ hal::tiva::Pwm::GeneratorIndex::generator1, uint8_t{ 0x00 }, uint8_t{ static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator0) | static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator1) }, true, uint16_t{ 0 } },
                    hal::tiva::Pwm::Config::InterruptConfig::FaultConfig{ hal::tiva::Pwm::GeneratorIndex::generator2, uint8_t{ 0x00 }, uint8_t{ static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator0) | static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator1) }, true, uint16_t{ 0 } },
                    hal::tiva::Pwm::Config::InterruptConfig::FaultConfig{ hal::tiva::Pwm::GeneratorIndex::generator3, uint8_t{ 0x00 }, uint8_t{ static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator0) | static_cast<uint8_t>(hal::tiva::Pwm::FaultInputComparator::comparator1) }, true, uint16_t{ 0 } },
                } },
                hal::InterruptPriority::Normal,
            };

            hal::tiva::Pwm::Config pwmConfig{ false, false, controlConfig, clockDivisor, std::make_optional(deadTimeConfig), std::make_optional(interruptConfig) };
        };

        struct SyncPwmConfig
        {
            hal::tiva::SynchronousPwm::Config::ClockDivisor clockDivisor{ hal::tiva::SynchronousPwm::Config::ClockDivisor::divisor8 };
            hal::tiva::SynchronousPwm::Config::Control controlConfig{ hal::tiva::SynchronousPwm::Config::Control::Mode::centerAligned, hal::tiva::SynchronousPwm::Config::Control::UpdateMode::globally, false };
            hal::tiva::SynchronousPwm::Config::DeadTime deadTimeConfig{ hal::tiva::SynchronousPwm::CalculateDeadTimeCycles(1000ns, clockDivisor), hal::tiva::SynchronousPwm::CalculateDeadTimeCycles(1000ns, clockDivisor) };
            hal::tiva::SynchronousPwm::Config pwmConfig{ false, false, controlConfig, clockDivisor, std::make_optional(deadTimeConfig) };
        };

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
            Peripherals() {};

            hal::OutputPin performance{ Pins::performance };
            Cortex cortex;
            TerminalAndTracer terminalAndTracer;
            AdcForPowerSupplyMeasurementImpl adcForPowerSupplyMeasurementImpl;
            AdcForPhaseCurrentMeasurementImpl adcForPhaseCurrentMeasurementImpl;
            AsyncPwmConfig asyncPwmConfig;
            SyncPwmConfig syncPwmConfig;
            hal::tiva::Eeprom eepromPeripheral;

            std::optional<AdcPhaseCurrentMeasurementImpl<hal::tiva::Adc>> phaseCurrentAdc;
            std::optional<hal::tiva::Pwm> asyncPwm;
            std::optional<hal::tiva::SynchronousPwm> syncPwm;
            std::optional<QuadratureEncoderDecoratorImpl<hal::tiva::QuadratureEncoder>> encoder;
            std::optional<CanBusAdapterImpl<hal::tiva::Can::WithMaxRxBuffer<32>>> canBus;

            struct PerformanceTrackerImpl
                : hal::PerformanceTracker
            {
                explicit PerformanceTrackerImpl(hal::DataWatchPointAndTrace& dwt, hal::OutputPin& pin);
                void Start() override;
                uint32_t ElapsedCycles() override;

                hal::DataWatchPointAndTrace& dwt;
                hal::OutputPin& pin;
            } performanceTrackerImpl{ cortex.dataWatchPointAndTrace, performance };
        };

    private:
        infra::Function<void()> onInitialized;
        PendSvLowPriorityInterrupt pendSvLowPriorityInterrupt;
        ResetCause resetCause{ ResetCause::powerUp };
        infra::BoundedString::WithStorage<1024> faultStatusString;
        hal::Hertz pwmBaseFrequency{ 10000 };
        foc::Radians encoderOffset{ 0.0f };
        infra::Function<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
        infra::Function<void(BoardProtectionReason)> onFaultCallback;
        std::optional<Peripherals> peripherals;
    };
}
