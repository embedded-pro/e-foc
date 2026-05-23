#pragma once

#include <algorithm>
#include <array>
#include HARDWARE_PINS_AND_PERIPHERALS_HEADER
#include "core/platform_abstraction/AdcPhaseCurrentMeasurement.hpp"
#include "core/platform_abstraction/CanBusAdapter.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/platform_abstraction/QuadratureEncoderDecorator.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/Pwm.hpp"
#include "hal/interfaces/SerialCommunication.hpp"
#include "hal/synchronous_interfaces/SynchronousQuadratureEncoder.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"

namespace application
{
    class PlatformFactoryImpl
        : public PlatformFactory
        , public hal::PerformanceTracker
        , public foc::LowPriorityInterrupt
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
        void Reset() override;
        ResetCause GetResetCause() const override;
        infra::BoundedConstString FaultStatus() const override;

        void RegisterBoardProtection(const infra::Function<void(PlatformFactory::BoardProtectionReason)>&) override
        {}

        // Implementation of hal::PerformanceTracker (Start also satisfies ThreePhaseInverter::Start — no-op on ST)
        void Start() override;
        uint32_t ElapsedCycles() override;

        // Implementation of LowPriorityInterrupt
        void Trigger() override;
        void Register(const infra::Function<void()>& handler) override;

        // Implementation of foc::ThreePhaseInverter
        void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

        // Implementation of foc::Encoder
        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

    private:
        struct PendSvLowPriorityInterrupt
            : public foc::LowPriorityInterrupt
        {
            void Trigger() override;
            void Register(const infra::Function<void()>& handler) override;

            infra::Function<void()> onLowPriorityInterrupt;
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

        class AdcMultiChannelStub
            : public hal::AdcMultiChannel
        {
        public:
            // Implementation of hal::AdcMultiChannel
            void Measure(const infra::Function<void(Samples)>& onDone) override;
            void Stop() override;
        };

        class ThreeChannelsPwmStub
            : public hal::ThreeChannelsPwm
        {
        public:
            // Implementation of hal::ThreeChannelsPwm
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

        class EepromStub
            : public hal::Eeprom
        {
        public:
            static constexpr uint32_t stubSize = 512;

            EepromStub()
            {
                storage.fill(0xFF);
            }

            uint32_t Size() const override
            {
                return stubSize;
            }

            void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override
            {
                const auto start = static_cast<decltype(storage.size())>(address);
                const auto length = buffer.size();

                if (start > storage.size() || length > storage.size() - start)
                {
                    onDone();
                    return;
                }

                std::copy(buffer.begin(), buffer.end(), storage.begin() + start);
                onDone();
            }

            void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override
            {
                const auto start = static_cast<decltype(storage.size())>(address);
                const auto length = buffer.size();

                if (start > storage.size() || length > storage.size() - start)
                {
                    onDone();
                    return;
                }

                std::copy(storage.begin() + start, storage.begin() + start + length, buffer.begin());
                onDone();
            }

            void Erase(infra::Function<void()> onDone) override
            {
                storage.fill(0xFF);
                onDone();
            }

        private:
            std::array<uint8_t, stubSize> storage;
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
        PendSvLowPriorityInterrupt pendSvLowPriorityInterrupt;
        static constexpr uint32_t timerId = 1;
        GpioPinStub pin;
        SerialCommunicationStub serial;
        TerminalAndTracer terminalAndTracer{ serial };
        std::optional<AdcPhaseCurrentMeasurementImpl<AdcMultiChannelStub>> phaseCurrentAdc;
        std::optional<ThreeChannelsPwmStub> pwm;
        std::optional<QuadratureEncoderDecoratorImpl<SynchronousQuadratureEncoderStub>> encoder;
        std::optional<CanBusAdapterImpl<CanStub>> canBus;
        EepromStub eepromStub;
        ResetCause resetCause{ ResetCause::powerUp };
        infra::BoundedString::WithStorage<1024> faultStatusString;
        hal::Hertz pwmBaseFrequency{ 10000 };
        foc::Radians encoderOffset{ 0.0f };
        infra::Function<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
    };
}
