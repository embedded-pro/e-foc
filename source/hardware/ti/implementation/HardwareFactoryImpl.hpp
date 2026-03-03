#pragma once

#include <optional>
#include HARDWARE_PINS_AND_PERIPHERALS_HEADER
#include "hal/interfaces/Gpio.hpp"
#include "hal/ti/hal_tiva/cortex/DataWatchpointAndTrace.hpp"
#include "hal/ti/hal_tiva/cortex/SystemTickTimerService.hpp"
#include "hal/ti/hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal/ti/hal_tiva/synchronous_tiva/SynchronousQuadratureEncoder.hpp"
#include "hal/ti/hal_tiva/tiva/Adc.hpp"
#include "hal/ti/hal_tiva/tiva/Uart.hpp"
#include "hal_tiva/synchronous_tiva/SynchronousAdc.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "services/tracer/SerialCommunicationOnSeggerRtt.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"
#include "source/hardware/HardwareFactory.hpp"

namespace application
{
    using namespace std::chrono_literals;

    class HardwareFactoryImpl
        : public HardwareFactory
        , public hal::PerformanceTracker
    {
    public:
        explicit HardwareFactoryImpl(const infra::Function<void()>& onInitialized);

        // Implementation of HardwareFactory
        void Run() override;
        services::Tracer& Tracer() override;
        services::TerminalWithCommands& Terminal() override;
        infra::MemoryRange<hal::GpioPin> Leds() override;
        hal::PerformanceTracker& PerformanceTimer() override;
        hal::Hertz SystemClock() const override;
        foc::Volts PowerSupplyVoltage() override;
        foc::Ampere MaxCurrentSupported() override;
        foc::LowPriorityInterrupt& LowPriorityInterrupt() override;
        infra::CreatorBase<hal::SynchronousThreeChannelsPwm, void(std::chrono::nanoseconds deadTime, hal::Hertz frequency)>& SynchronousThreeChannelsPwmCreator() override;
        infra::CreatorBase<AdcPhaseCurrentMeasurement, void(SampleAndHold)>& AdcMultiChannelCreator() override;
        infra::CreatorBase<QuadratureEncoderDecorator, void()>& SynchronousQuadratureEncoderCreator() override;

        // Implementation of hal::PerformanceTracker
        void Start() override;
        uint32_t ElapsedCycles() override;

    private:
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
            hal::tiva::Uart::Config uartConfig{ true, true, hal::tiva::Uart::Baudrate::_921000_bps, hal::tiva::Uart::FlowControl::none, hal::tiva::Uart::Parity::none, hal::tiva::Uart::StopBits::one, hal::tiva::Uart::NumberOfBytes::_8_bytes, infra::none };
            hal::tiva::Uart uart{ Peripheral::UartIndex, Pins::uartTx, Pins::uartRx, uartConfig };
            services::SerialCommunicationOnSeggerRtt serialCommunicationOnSeggerRtt;
            services::StreamWriterOnSerialCommunication::WithStorage<2048> streamWriterOnSerialCommunication{ serialCommunicationOnSeggerRtt };
            infra::TextOutputStream::WithErrorPolicy tracerStream{ streamWriterOnSerialCommunication };
            services::TracerWithDateTime tracer{ tracerStream };
            services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<256, 10> terminal{ serialCommunicationOnSeggerRtt, tracer };
        };

        struct MotorFieldOrientedControllerInterfaceImpl
        {
            const std::array<hal::tiva::Adc::SampleAndHold, 5> toSampleAndHold{ { hal::tiva::Adc::SampleAndHold::sampleAndHold4,
                hal::tiva::Adc::SampleAndHold::sampleAndHold16,
                hal::tiva::Adc::SampleAndHold::sampleAndHold32,
                hal::tiva::Adc::SampleAndHold::sampleAndHold64,
                hal::tiva::Adc::SampleAndHold::sampleAndHold256 } };
            static constexpr float voltageToCurrent = 5.0f;
            static constexpr float adcReferenceVoltage = 3.3f;
            static constexpr float adcResolution = 4096.0f;
            static constexpr float voltageToVolts = 18.433f;
            static constexpr float adcToAmpereSlope = (adcReferenceVoltage / adcResolution) * voltageToCurrent;
            static constexpr float adcToAmpereOffset = -(adcReferenceVoltage / 2.0f) * voltageToCurrent; // adc midpoint reference
            static constexpr float adcToVoltsFactor = (adcReferenceVoltage / adcResolution) * voltageToVolts;
            static constexpr hal::tiva::Adc::SamplingDelay phaseDelay{ 4 };
            static constexpr auto currentSensingOversampling = hal::tiva::Adc::Oversampling::oversampling2;
            hal::tiva::Adc::Config adcConfig{ false, 0, Peripheral::adcTrigger, hal::tiva::Adc::SampleAndHold::sampleAndHold8, std::make_optional(currentSensingOversampling), phaseDelay };
            std::array<hal::tiva::AnalogPin, 3> currentPhaseAnalogPins{ { hal::tiva::AnalogPin{ Pins::currentPhaseA }, hal::tiva::AnalogPin{ Pins::currentPhaseB }, hal::tiva::AnalogPin{ Pins::currentPhaseC } } };
            infra::Creator<AdcPhaseCurrentMeasurement, AdcPhaseCurrentMeasurementImpl<hal::tiva::Adc>, void(SampleAndHold)> adcCurrentPhases{ [this](auto& object, auto sampleAndHold)
                {
                    adcConfig.sampleAndHold = toSampleAndHold.at(static_cast<std::size_t>(sampleAndHold));

                    object.Emplace(adcToAmpereSlope, adcToAmpereOffset, Peripheral::AdcIndex, Peripheral::AdcSequencerIndex, currentPhaseAnalogPins, adcConfig);
                } };
            hal::tiva::SynchronousPwm::Config::ClockDivisor clockDivisor{ hal::tiva::SynchronousPwm::Config::ClockDivisor::divisor8 };
            hal::tiva::SynchronousPwm::Config::Control controlConfig{ hal::tiva::SynchronousPwm::Config::Control::Mode::centerAligned, hal::tiva::SynchronousPwm::Config::Control::UpdateMode::globally, false };
            hal::tiva::SynchronousPwm::Config::DeadTime deadTimeConfig{ hal::tiva::SynchronousPwm::CalculateDeadTimeCycles(1000ns, clockDivisor), hal::tiva::SynchronousPwm::CalculateDeadTimeCycles(1000ns, clockDivisor) };
            hal::tiva::SynchronousPwm::Config pwmConfig{ false, false, controlConfig, clockDivisor, std::make_optional(deadTimeConfig) };
            infra::Creator<hal::SynchronousThreeChannelsPwm, hal::tiva::SynchronousPwm, void(std::chrono::nanoseconds deadTime, hal::Hertz frequency)> pwmBrushless{ [this](auto& object, auto deadTime, auto frequency)
                {
                    deadTimeConfig.fallInClockCycles = hal::tiva::SynchronousPwm::CalculateDeadTimeCycles(deadTime, clockDivisor);
                    deadTimeConfig.riseInClockCycles = hal::tiva::SynchronousPwm::CalculateDeadTimeCycles(deadTime, clockDivisor);

                    pwmConfig.deadTime = std::make_optional(deadTimeConfig);

                    object.Emplace(Peripheral::PwmIndex, Peripheral::pwmPhases, pwmConfig);
                    object->SetBaseFrequency(frequency);
                } };
            infra::Function<void(std::tuple<infra::Ampere, infra::Ampere, infra::Ampere> voltagePhases)> phaseCurrentsReady;
            std::array<hal::tiva::AnalogPin, 1> powerSupplyAnalogPins{ { hal::tiva::AnalogPin{ Pins::powerSupplyVoltage } } };
            constexpr static auto powerSupplyOversampling = hal::tiva::SynchronousAdc::Oversampling::oversampling8;
            hal::tiva::SynchronousAdc::Config powerSupplyAdcConfig{ hal::tiva::SynchronousAdc::SampleAndHold::sampleAndHold256, hal::tiva::SynchronousAdc::Priority::priority3, std::make_optional(powerSupplyOversampling) };
            hal::tiva::SynchronousAdc powerSupplyAdc{ 1, 0, powerSupplyAnalogPins, powerSupplyAdcConfig };
        };

        struct EncoderImpl
        {
            using Conf = hal::tiva::QuadratureEncoder::Config;

            static constexpr uint32_t resolution = 4000;
            hal::tiva::QuadratureEncoder::Config qeiConfig{ resolution, 0, false, false, false, Conf::ResetMode::onMaxPosition, Conf::CaptureMode::phaseAandPhaseB, Conf::SignalMode::quadrature };
            infra::Creator<QuadratureEncoderDecorator, QuadratureEncoderDecoratorImpl<hal::tiva::QuadratureEncoder>, void()> synchronousQuadratureEncoderCreator{ [this](auto& object)
                {
                    object.Emplace(resolution, Peripheral::QeiIndex, Pins::encoderA, Pins::encoderB, Pins::encoderZ, qeiConfig);
                } };
        };

        struct Peripherals
        {
            Peripherals() {};

            hal::OutputPin performance{ Pins::performance };
            Cortex cortex;
            TerminalAndTracer terminalAndTracer;
            EncoderImpl encoderImpl;
            MotorFieldOrientedControllerInterfaceImpl motorFieldOrientedController;
        };

    private:
        infra::Function<void()> onInitialized;
        PendSvLowPriorityInterrupt pendSvLowPriorityInterrupt;
        std::optional<Peripherals> peripherals;
    };
}
