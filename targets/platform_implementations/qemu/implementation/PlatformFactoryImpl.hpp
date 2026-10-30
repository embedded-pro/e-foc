#pragma once

#include "core/foc/model/OneShotDelay.hpp"
#include "core/foc/model/PlantResponseRecorder.hpp"
#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/platform_abstraction/CanBusAdapter.hpp"
#include "core/platform_abstraction/LowSideShuntWindow.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/cortex_m/SystemTickTimerService.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/qemu/async/WatchdogQemu.hpp"
#include "infra/stream/OutputStream.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/HotSwapFunction.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"
#include "services/util/EventDispatcherWatchdog.hpp"
#include "services/util/Terminal.hpp"
#include "targets/platform_implementations/cortex_m_common/EventDispatcherCortexWithWeakPtr.hpp"
#include "targets/platform_implementations/cortex_m_common/FocLowPriorityInterruptAdapter.hpp"
#include "targets/platform_implementations/qemu/implementation/BoardProtectionSimulator.hpp"
#include "targets/platform_implementations/qemu/implementation/CmsdkUartLines.hpp"
#include "targets/platform_implementations/qemu/implementation/EmulatedCanBusAdapter.hpp"
#include "targets/platform_implementations/qemu/implementation/QemuConstants.hpp"
#include "targets/platform_implementations/qemu/implementation/QemuCycleCounter.hpp"
#include "targets/platform_implementations/qemu/implementation/QemuTimer.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingEeprom.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingPlantConfig.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingRecordFile.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingSerial.hpp"
#include "targets/platform_implementations/qemu/implementation/SilWatchdogStall.hpp"
#include <atomic>
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
        hal::Hertz SystemClock() const override;
        foc::Volts PowerSupplyVoltage() override;
        foc::LowPriorityInterrupt& LowPriorityInterrupt() override;
        hal::Eeprom& Eeprom() override;
        void RegisterBoardProtection(const infra::Function<void(BoardProtectionReason)>& onProtection) override;
        BoardProtectionState BoardProtectionStatus() override;
        void Reset() override;
        ResetCause GetResetCause() const override;
        infra::BoundedConstString FaultStatus() const override;
        PlatformDiagnostics& Diagnostics() override;

        void PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

    private:
        class GpioPinStub
            : public hal::GpioPin
        {
        public:
            bool Get() const override
            {
                return pinState;
            }

            void Set(bool value) override
            {
                pinState = value;
            }

            bool GetOutputLatch() const override
            {
                return pinState;
            }

            void SetAsInput() override
            {}

            bool IsInput() const override
            {
                return false;
            }

            void Config(hal::PinConfigType) override
            {}

            void Config(hal::PinConfigType, bool startOutputState) override
            {
                pinState = startOutputState;
            }

            void ResetConfig() override
            {}

            void EnableInterrupt(const infra::Function<void()>&, hal::InterruptTrigger, hal::InterruptType) override
            {}

            void DisableInterrupt() override
            {}

        private:
            bool pinState{ false };
        };

        using EventDispatcherCortexWithWeakPtrAndWatchdog = infra::EventDispatcherWithWeakPtrConnector<services::EventDispatcherWatchdogWorker<EventDispatcherCortexWithWeakPtrWorker>>;

        struct Cortex
        {
            static constexpr infra::Duration watchdogEarlyWarningPeriod{ std::chrono::milliseconds(25) };
            static constexpr infra::Duration watchdogExpirationTimeout{ std::chrono::milliseconds(100) };

            static void OnWatchdogExpired();

            hal::cortex::InterruptTable::WithStorage<64> interruptTable;
            hal::WatchdogQemu watchdog{ hal::WatchdogQemu::Config{ .timeout = watchdogEarlyWarningPeriod } };
            // The TI boards' depth, so the SIL suite runs the application within it
            EventDispatcherCortexWithWeakPtrAndWatchdog::WithSize<24> eventDispatcher{ watchdog, watchdogExpirationTimeout, []()
                {
                    OnWatchdogExpired();
                } };
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
        foc::PlantResponseSample PlantSampleAt(uint32_t tick) const;
        void PollPorts();
        void DrainResponseRecords();
        void RecordResponse(const foc::PlantResponseRecord& record);
        void StampReceivedFrame(uint32_t tick, const sil::EmulatedCan::Frame& frame);

    private:
        static foc::ThreePhaseMotorModel::Parameters MotorParametersFrom(
            const std::optional<sil::SilPlantConfig>& config,
            const foc::ThreePhaseMotorModel::Parameters& fallback);
        static foc::Volts SupplyVoltageFrom(const std::optional<sil::SilPlantConfig>& config);
        static hal::Hertz BaseFrequencyFrom(const std::optional<sil::SilPlantConfig>& config);

        void ApplyPlantConfig();
        void ApplyCurrentSense(SampleAndHold requested);

        const std::optional<sil::SilPlantConfig> plantConfig;
        const ResetCause resetCause;
        infra::Function<void()> onInitialized;
        QemuCycleCounter cycleCounter;
        ControlLoopMetrics controlLoopMetrics;
        PlatformDiagnostics diagnostics{ controlLoopMetrics };
        volatile bool controlLoopEntered{ false };
        FocLowPriorityInterruptAdapter pendSvLowPriorityInterrupt;
        Cortex cortex;
        QemuTimer focTimer;
        SemihostingSerial serial;
        sil::CmsdkUartLines terminalPort{ kTerminalUartBase };
        TerminalAndTracerBlock terminalAndTracer{ serial };
        SilWatchdogStall watchdogStall{ terminalAndTracer.tracer };
        SemihostingEeprom eeprom{ "eeprom.bin" };
        GpioPinStub operationalPin;
        GpioPinStub warningPin;
        GpioPinStub failurePin;
        BoardProtectionSimulator boardProtection;
        foc::ThreePhaseMotorModel model;
        foc::PlantResponseRecorder<1024> responseRecorder;
        SemihostingRecordFile plantRecord{ "plant.rec" };
        foc::OneShotDelay torqueStep;
        foc::NewtonMeter torqueStepTorque{ 0.0f };
        foc::OneShotDelay encoderFreeze;
        foc::ThreePhaseMotorModel::FaultInjectionConfig bootFaultInjection{};
        std::atomic<uint32_t> controlTick{ 0 };
        std::optional<EmulatedCanBusAdapter> canBusAdapter;
        infra::TimerRepeating portPollTimer;
        infra::TimerRepeating protectionPollTimer;
        infra::TimerRepeating responseDrainTimer;
        hal::Hertz baseFrequency;
        infra::HotSwapFunction<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
        foc::PhasePwmDutyCycles lastDutyPhases{};
        foc::PhasePwmDutyCycles appliedDutyPhases{};
        bool dutyDelayed{ false };
        hal::DutyCycle maxReadableDuty{ hal::DutyCycle::FromPercent(100) };
        hal::DutyCycle maxDuty{ hal::DutyCycle::FromPercent(100) };
        foc::LowSideShuntReadout shuntReadout{ maxReadableDuty };
        foc::PhaseCurrents lastCurrents{};
    };
}
