#pragma once

#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/SerialCommunication.hpp"
#include "hal/synchronous_interfaces/SynchronousAdc.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"
#include "source/hardware/HardwareFactory.hpp"

namespace application
{
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
        infra::CreatorBase<CanBusAdapter, void(uint32_t bitRate, bool testMode)>& CanBusCreator() override;

        // Implementation of hal::PerformanceTracker
        void Start() override;
        uint32_t ElapsedCycles() override;

    private:
        class SimpleLowPriorityInterrupt
            : public foc::LowPriorityInterrupt
        {
        public:
            void Trigger() override
            {
                if (handler)
                    handler();
            }

            void Register(const infra::Function<void()>& handler) override
            {
                this->handler = handler;
            }

        private:
            infra::Function<void()> handler;
        };

        class AdcMultiChannelStub
            : public hal::AdcMultiChannel
        {
        public:
            // Implementation of hal::AdcMultiChannel
            void Measure(const infra::Function<void(Samples)>& onDone) override;
            void Stop() override;
        };

        class SynchronousThreeChannelsPwmStub
            : public hal::SynchronousThreeChannelsPwm
        {
        public:
            // Implementation of hal::SynchronousThreeChannelsPwm
            void SetBaseFrequency(hal::Hertz baseFrequency) override;
            void Stop() override;
            void Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2, hal::Percent dutyCycle3) override;
        };

        class SynchronousQuadratureEncoderStub
            : public hal::SynchronousQuadratureEncoder
        {
        public:
            uint32_t Position() override;
            uint32_t Resolution() override;
            MotionDirection Direction() override;
            uint32_t Speed() override;
        };

        class CanStub
            : public hal::Can
        {
        public:
            void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
            void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;
        };

        class SynchronousAdcStub
            : public hal::SynchronousAdc
        {
        public:
            Samples Measure(std::size_t numberOfSamples) override
            {
                return Samples();
            }
        };

        class SerialCommunicationStub
            : public hal::SerialCommunication
        {
        public:
            void SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion) override;
            void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;
        };

        class GpioPinStub
            : public hal::GpioPin
        {
        public:
            bool Get() const override;
            void Set(bool value) override;
            bool GetOutputLatch() const override;
            void SetAsInput() override;
            bool IsInput() const override;
            void Config(hal::PinConfigType config) override;
            void Config(hal::PinConfigType config, bool startOutputState) override;
            void ResetConfig() override;
            void EnableInterrupt(const infra::Function<void()>& action, hal::InterruptTrigger trigger, hal::InterruptType type) override;
            void DisableInterrupt() override;
        };

        struct TerminalAndTracer
        {
            explicit TerminalAndTracer(hal::SerialCommunication& com)
                : streamWriterOnSerialCommunication(com)
                , tracerStream(streamWriterOnSerialCommunication)
                , tracer(tracerStream)
                , terminal(com, tracer)
            {}

            services::StreamWriterOnSerialCommunication::WithStorage<8192> streamWriterOnSerialCommunication;
            infra::TextOutputStream::WithErrorPolicy tracerStream;
            services::TracerWithDateTime tracer;
            services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<> terminal;
        };

    private:
        infra::Function<void()> onInitialized;
        SimpleLowPriorityInterrupt simpleLowPriorityInterrupt;
        GpioPinStub pin;
        SerialCommunicationStub serial;
        TerminalAndTracer terminalAndTracer{ serial };
        infra::Creator<AdcPhaseCurrentMeasurement, AdcPhaseCurrentMeasurementImpl<AdcMultiChannelStub>, void(SampleAndHold)> adcCurrentPhases{ [this](auto& object, auto)
            {
                object.emplace(1.0f, 0.0f);
            } };
        infra::Creator<hal::SynchronousThreeChannelsPwm, SynchronousThreeChannelsPwmStub, void(std::chrono::nanoseconds deadTime, hal::Hertz frequency)> pwmBrushless{ [this](auto& object, auto, auto)
            {
                object.emplace();
            } };
        infra::Creator<QuadratureEncoderDecorator, QuadratureEncoderDecoratorImpl<SynchronousQuadratureEncoderStub>, void()> synchronousQuadratureEncoderCreator{ [this](auto& object)
            {
                object.emplace(1);
            } };
        infra::Creator<CanBusAdapter, CanBusAdapterImpl<CanStub>, void(uint32_t bitRate, bool testMode)> canCreator{ [this](auto& object, auto, auto)
            {
                object.emplace();
            } };
    };
}
