#pragma once

#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/platform_abstraction/CanBusAdapter.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "hal/cortex_m/EventDispatcherCortex.hpp"
#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/cortex_m/SystemTickTimerService.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "infra/stream/OutputStream.hpp"
#include "infra/timer/Timer.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"
#include "services/util/Terminal.hpp"
#include "targets/platform_implementations/cortex_m_common/FocLowPriorityInterruptAdapter.hpp"
#include "targets/platform_implementations/qemu/implementation/ArrayEeprom.hpp"
#include "targets/platform_implementations/qemu/implementation/QemuTimer.hpp"
#include "targets/platform_implementations/qemu/implementation/QemuConstants.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingCanBusAdapter.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingSerial.hpp"
#include <optional>

namespace application
{
    class PlatformFactoryImpl
        : public PlatformFactory
    {
    public:
        explicit PlatformFactoryImpl(const foc::ThreePhaseMotorModel::Parameters& motorParams,
                                     const infra::Function<void()>& onInitialized);

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
        void RegisterBoardProtection(const infra::Function<void(BoardProtectionReason)>& onProtection) override;
        void Reset() override;
        ResetCause GetResetCause() const override;
        infra::BoundedConstString FaultStatus() const override;

        OPTIMIZE_FOR_SPEED void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents)>& onDone) override;
        OPTIMIZE_FOR_SPEED void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

        OPTIMIZE_FOR_SPEED foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

    private:
        class GpioPinStub
            : public hal::GpioPin
        {
        public:
            bool Get() const override { return pinState; }
            void Set(bool value) override { pinState = value; }
            bool GetOutputLatch() const override { return pinState; }
            void SetAsInput() override {}
            bool IsInput() const override { return false; }
            void Config(hal::PinConfigType) override {}
            void Config(hal::PinConfigType, bool startOutputState) override { pinState = startOutputState; }
            void ResetConfig() override {}
            void EnableInterrupt(const infra::Function<void()>&, hal::InterruptTrigger, hal::InterruptType) override {}
            void DisableInterrupt() override {}

        private:
            bool pinState{ false };
        };

        class NoOpPerformanceTracker
            : public hal::PerformanceTracker
        {
        public:
            void Start() override {}
            uint32_t ElapsedCycles() override { return 0; }
        };

        struct Cortex
        {
            hal::cortex::InterruptTable::WithStorage<64> interruptTable;
            hal::cortex::EventDispatcherCortex::WithSize<50> eventDispatcher;
            hal::cortex::SystemTickTimerService systemTick{ kQemuSystemClockHz, std::chrono::milliseconds(1) };
        };

        struct TerminalAndTracerBlock
        {
            explicit TerminalAndTracerBlock(hal::SerialCommunication& serial)
                : streamWriter(serial)
                , tracerStream(streamWriter)
                , tracer(tracerStream)
                , terminal(serial, tracer)
            {}

            services::StreamWriterOnSerialCommunication::WithStorage<4096> streamWriter;
            infra::TextOutputStream::WithErrorPolicy tracerStream;
            services::TracerWithDateTime tracer;
            services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<32, 4> terminal;
        };

        void FocTimerIsr();

    private:
        infra::Function<void()> onInitialized;
        FocLowPriorityInterruptAdapter pendSvLowPriorityInterrupt;
        Cortex cortex;
        QemuTimer focTimer{ 0x40000000u, 8, kQemuSystemClockHz, 20000u, [this]() { FocTimerIsr(); } };
        SemihostingSerial serial;
        TerminalAndTracerBlock terminalAndTracer{ serial };
        ArrayEeprom eeprom;
        GpioPinStub operationalPin;
        GpioPinStub warningPin;
        GpioPinStub failurePin;
        NoOpPerformanceTracker performanceTracker;
        foc::ThreePhaseMotorModel model;
        std::optional<SemihostingCanBusAdapter> canBusAdapter;
        infra::TimerRepeating canPollTimer;
        hal::Hertz baseFrequency{ 20000 };
        infra::Function<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
        foc::PhasePwmDutyCycles lastDutyPhases{};
        foc::PhaseCurrents lastCurrents{};
    };
}
