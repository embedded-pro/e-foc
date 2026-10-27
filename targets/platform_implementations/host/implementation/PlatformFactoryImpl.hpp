#pragma once

#include "core/platform_abstraction/AdcPhaseCurrentMeasurement.hpp"
#include "core/platform_abstraction/CanBusAdapter.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/platform_abstraction/QuadratureEncoderDecorator.hpp"
#include "hal/generic/TimerServiceGeneric.hpp"
#include "hal/interfaces/Can.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/Pwm.hpp"
#include "hal/interfaces/SerialCommunication.hpp"
#include "hal/synchronous_interfaces/SynchronousAdc.hpp"
#include "hal/synchronous_interfaces/SynchronousQuadratureEncoder.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"
#include "targets/platform_implementations/peripheral_stubs/PeripheralStubs.hpp"
#include <algorithm>
#include <array>
#include <ranges>
#include <span>

namespace application
{
    class PlatformFactoryImpl
        : public PlatformFactory
        , public hal::PerformanceTracker
    {
    public:
        explicit PlatformFactoryImpl(const infra::Function<void()>& onInitialized);

        void ConfigureAdcAndPwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime, SampleAndHold sampleAndHold) override;
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
        void Reset() override;
        ResetCause GetResetCause() const override;
        infra::BoundedConstString FaultStatus() const override;
        PlatformDiagnostics& Diagnostics() override;

        // Also satisfies ThreePhaseInverter::Start; no-op on host
        void Start() override;
        uint32_t ElapsedCycles() override;

        void RegisterBoardProtection(const infra::Function<void(PlatformFactory::BoardProtectionReason)>& onProtection) override
        {
            onBoardProtection = onProtection;
        }

        PlatformFactory::BoardProtectionState BoardProtectionStatus() override
        {
            return PlatformFactory::BoardProtectionState::unknown;
        }

        void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

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

            void Unregister() override
            {
                handler = nullptr;
            }

        private:
            infra::Function<void()> handler;
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
                really_assert(address <= stubSize);
                really_assert(buffer.size() <= stubSize - address);

                std::ranges::copy(buffer, storage.begin() + address);
                onDone();
            }

            void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override
            {
                really_assert(address <= stubSize);
                really_assert(buffer.size() <= stubSize - address);
                std::ranges::copy(std::span(storage.begin() + address, buffer.size()), buffer.begin());
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

        class SynchronousAdcStub
            : public hal::SynchronousAdc
        {
        public:
            Samples Measure(std::size_t) override
            {
                return Samples();
            }
        };

        struct LedPins
        {
            GpioPinStub operational;
            GpioPinStub warning;
            GpioPinStub failure;
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
        // Constructed before onInitialized() runs, so anything it schedules or times during
        // Logic's construction finds a live TimerService / EventDispatcherWithWeakPtr::Instance().
        hal::TimerServiceGeneric timerService;
        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;
        infra::Function<void()> onInitialized;
        SimpleLowPriorityInterrupt simpleLowPriorityInterrupt;
        LedPins ledPins;
        SerialCommunicationStub serial;
        TerminalAndTracer terminalAndTracer{ serial };
        std::optional<AdcPhaseCurrentMeasurementImpl<AdcMultiChannelStub>> phaseCurrentAdc;
        std::optional<ThreeChannelsPwmStub> pwm;
        std::optional<QuadratureEncoderDecoratorImpl<SynchronousQuadratureEncoderStub>> encoder;
        std::optional<CanBusAdapterImpl<CanStub>> canBus;
        EepromStub eepromStub;
        ResetCause resetCause{ ResetCause::powerUp };
        ControlLoopMetrics controlLoopMetrics;
        PlatformDiagnostics diagnostics{ controlLoopMetrics };
        infra::BoundedString::WithStorage<1024> faultStatusString;
        hal::Hertz pwmBaseFrequency{ 20000 };
        foc::Radians encoderOffset{ 0.0f };
        infra::Function<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
        infra::Function<void(PlatformFactory::BoardProtectionReason)> onBoardProtection;
    };
}
