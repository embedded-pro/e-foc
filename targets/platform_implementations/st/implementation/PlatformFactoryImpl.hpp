#pragma once

#include <algorithm>
#include <array>
#include HARDWARE_PINS_AND_PERIPHERALS_HEADER
#include MOTOR_BOARD_CHARACTERISTICS_HEADER
#include "core/platform_abstraction/AdcPhaseCurrentMeasurement.hpp"
#include "core/platform_abstraction/CanBusAdapter.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/platform_abstraction/QuadratureEncoderDecorator.hpp"
#include "hal/cortex_m/SystemTickTimerService.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/Pwm.hpp"
#include "hal/interfaces/SerialCommunication.hpp"
#include "hal/synchronous_interfaces/SynchronousQuadratureEncoder.hpp"
#include "hal_st/stm32fxxx/WatchDogStm.hpp"
#include "infra/util/HotSwapFunction.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"
#include "services/util/EventDispatcherWatchdog.hpp"
#include "targets/platform_implementations/cortex_m_common/FocLowPriorityInterruptAdapter.hpp"
#include "targets/platform_implementations/peripheral_stubs/PeripheralStubs.hpp"

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

        void RegisterBoardProtection(const infra::Function<void(PlatformFactory::BoardProtectionReason)>& onProtection) override
        {
            onBoardProtection = onProtection;
        }

        PlatformFactory::BoardProtectionState BoardProtectionStatus() override
        {
            return PlatformFactory::BoardProtectionState::unknown;
        }

        // Also satisfies ThreePhaseInverter::Start; no-op on ST
        void Start() override;
        uint32_t ElapsedCycles() override;

        void PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

    private:
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
        static constexpr infra::Duration watchdogExpirationTimeout{ std::chrono::milliseconds(100) };

        static hal::WatchDogStm::Config WatchdogConfig();
        static void OnWatchdogExpired();

        struct HalInitialization
        {
            HalInitialization();
        };

        // Initialised first, so the watchdog the dispatcher starts runs on an initialised HAL
        HalInitialization halInitialization;
        hal::cortex::SystemTickTimerService systemTick{ SystemCoreClock, std::chrono::milliseconds(1) };
        hal::WatchDogStm watchdog{ WatchdogConfig() };
        services::EventDispatcherWithWeakPtrAndWatchdog::WithSize<50> eventDispatcher{ watchdog, watchdogExpirationTimeout, []()
            {
                OnWatchdogExpired();
            } };
        infra::Function<void()> onInitialized;
        FocLowPriorityInterruptAdapter pendSvLowPriorityInterrupt;
        static constexpr uint32_t timerId = 1;
        GpioPinStub operationalPin;
        GpioPinStub warningPin;
        GpioPinStub failurePin;
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
        infra::HotSwapFunction<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
        infra::Function<void(PlatformFactory::BoardProtectionReason)> onBoardProtection;
    };
}
