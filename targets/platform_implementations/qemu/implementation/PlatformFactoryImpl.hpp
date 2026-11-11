#pragma once

#include "can-lite/core/MonitoredCan.hpp"
#include "core/foc/model/drive/BoardProtectionSimulator.hpp"
#include "core/foc/model/drive/PlantDrive.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/cortex_m/SystemTickTimerService.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/qemu/async/WatchdogQemu.hpp"
#include "hal/qemu/semihosting/SemihostingAsyncFlash.hpp"
#include "hal/qemu/semihosting/SemihostingEeprom.hpp"
#include "hal/qemu/semihosting/SemihostingFileFlash.hpp"
#include "hal/qemu/semihosting/SemihostingFileSystemCortex.hpp"
#include "hal/qemu/semihosting/SemihostingRecordFile.hpp"
#include "infra/stream/OutputStream.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/HotSwapFunction.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"
#include "services/util/EventDispatcherWatchdog.hpp"
#include "services/util/Terminal.hpp"
#include "targets/platform_implementations/cortex_m_common/EventDispatcherCortexWithWeakPtr.hpp"
#include "targets/platform_implementations/cortex_m_common/FocLowPriorityInterruptAdapter.hpp"
#include "targets/platform_implementations/qemu/implementation/CmsdkUartLines.hpp"
#include "targets/platform_implementations/qemu/implementation/EmulatedCanBusAdapter.hpp"
#include "targets/platform_implementations/qemu/implementation/QemuConstants.hpp"
#include "targets/platform_implementations/qemu/implementation/QemuCycleCounter.hpp"
#include "targets/platform_implementations/qemu/implementation/QemuTimer.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingPlantConfig.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingSerial.hpp"
#include "targets/platform_implementations/qemu/implementation/SilWatchdogStall.hpp"
#include <optional>

namespace application
{
    class PlatformFactoryImpl
        : public PlatformFactory
        , public drivers::EncoderIndex
    {
    public:
        explicit PlatformFactoryImpl(const foc::ThreePhaseMotorModel::Parameters& motorParams,
            const infra::Function<void()>& onInitialized);

        void ConfigureAdcAndPwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime, SampleAndHold sampleAndHold) override;
        drivers::DeadTimeLoss InverterDeadTimeLoss(std::chrono::nanoseconds deadTime) const override;
        uint32_t DutyUpdateDelayPeriods() const override;
        void SetEncoderResolution(uint32_t resolution) override;
        void ConfigureCanBus(uint32_t bitRate, bool testMode) override;
        services::MonitoredCan& CanBus() override;

        void Run() override;
        services::Tracer& Tracer() override;
        services::TerminalWithCommands& Terminal() override;
        hal::GpioPin& OperationalLed() override;
        hal::GpioPin& WarningLed() override;
        hal::GpioPin& FailureLed() override;
        uint8_t BoardId() const override;
        BoardKind GetBoardKind() const override;
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
        std::optional<services::FirmwareStaging> Staging() override;
        std::optional<std::reference_wrapper<drivers::EncoderIndex>> EncoderIndexChannel() override;

        void PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;
        void CurrentOffsets(const foc::PhaseCurrents& offsets) override;
        void ZeroVectorSlopes(const foc::PhaseCurrentSlopes& slopes) override;

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

        bool HasIndex() const override;
        void ArmIndex() override;
        bool IndexSeen() override;

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
        void PollPorts();
        void DrainResponseRecords();
        void RecordResponse(const foc::PlantResponseRecord& record);
        void StampReceivedFrame(uint32_t tick, const sil::EmulatedCan::Frame& frame);

    private:
        static hal::Hertz BaseFrequencyFrom(const std::optional<sil::SilPlantConfig>& config);

        void ApplyPlantConfig();

        const std::optional<sil::SilPlantConfig> plantConfig;
        const ResetCause resetCause;
        infra::Function<void()> onInitialized;
        QemuCycleCounter cycleCounter;
        infra::ExecutionTimeStatistics controlLoopMetrics;
        PlatformDiagnostics diagnostics{ controlLoopMetrics };
        volatile bool controlLoopEntered{ false };
        FocLowPriorityInterruptAdapter pendSvLowPriorityInterrupt;
        Cortex cortex;
        QemuTimer focTimer;
        SemihostingSerial serial;
        sil::CmsdkUartLines terminalPort{ kTerminalUartBase };
        TerminalAndTracerBlock terminalAndTracer{ serial };
        SilWatchdogStall watchdogStall{ terminalAndTracer.tracer };
        hal::SemihostingFileSystemCortex fileSystem;
        hal::SemihostingEeprom::WithStorage<512> eeprom{ fileSystem, "eeprom.bin" };
        hal::SemihostingAsyncFlash stagingFlash;
        hal::SemihostingFileFlash journalFlash;
        GpioPinStub operationalPin;
        GpioPinStub warningPin;
        GpioPinStub failurePin;
        BoardProtectionSimulator boardProtection;
        sil::PlantDrive drive;
        const foc::ThreePhaseMotorModel& plantModel{ drive.Model() };
        const foc::SimulatedEncoder& plantEncoder{ drive.Model().Encoder() };
        hal::SemihostingRecordFile::WithBuffer<1024> plantRecord{ fileSystem, "plant.rec" };
        std::optional<EmulatedCanBusAdapter> canBusAdapter;
        infra::TimerRepeating portPollTimer;
        infra::TimerRepeating protectionPollTimer;
        infra::TimerRepeating responseDrainTimer;
        infra::HotSwapFunction<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
    };
}
