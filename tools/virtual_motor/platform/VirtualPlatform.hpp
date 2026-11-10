#pragma once

#include "core/foc/model/drive/BoardProtectionSimulator.hpp"
#include "core/foc/model/drive/PlantDrive.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "infra/timer/TickOnInterruptTimerService.hpp"
#include "infra/util/ExecutionTimeStatistics.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"
#include "services/util/Terminal.hpp"
#include "tools/virtual_motor/platform/BoardPeripherals.hpp"
#include "tools/virtual_motor/platform/FileEeprom.hpp"
#include "tools/virtual_motor/platform/VirtualCanPort.hpp"
#include <string>

namespace virtual_motor
{
    struct BoardOptions
    {
        std::string bus;
        uint8_t node{ 0 };
        sil::SilPlantConfig plant{};
        std::string memoryFile;
        uint16_t hubPort{ services::loopback::virtualCanHubPort };
        bool restarted{ false };
        infra::Function<void()> onReset;
    };

    class VirtualPlatform
        : public application::PlatformFactory
        , public drivers::EncoderIndex
    {
    public:
        explicit VirtualPlatform(const BoardOptions& options);

        void AdvanceOneMillisecond();
        void EachMillisecond(const infra::Function<void()>& action);

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
        application::BoardKind GetBoardKind() const override;
        hal::Hertz SystemClock() const override;
        foc::Volts PowerSupplyVoltage() override;
        foc::LowPriorityInterrupt& LowPriorityInterrupt() override;
        hal::Eeprom& Eeprom() override;
        void RegisterBoardProtection(const infra::Function<void(BoardProtectionReason)>& onProtection) override;
        BoardProtectionState BoardProtectionStatus() override;
        application::PlatformDiagnostics& Diagnostics() override;
        void Reset() override;
        application::ResetCause GetResetCause() const override;
        infra::BoundedConstString FaultStatus() const override;
        std::optional<std::reference_wrapper<drivers::EncoderIndex>> EncoderIndexChannel() override;

        void PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;
        void CurrentOffsets(const foc::PhaseCurrents& offsets) override;

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

        bool HasIndex() const override;
        void ArmIndex() override;
        bool IndexSeen() override;

    private:
        struct Execution
        {
            infra::EventDispatcherWithWeakPtr::WithSize<256> eventDispatcher;
            infra::TickOnInterruptTimerService timerService{ infra::systemTimerServiceId, std::chrono::milliseconds(1) };
            DeferredLowPriorityInterrupt lowPriorityInterrupt;
        };

        struct Console
        {
            StandardOutputSerial serial;
            services::StreamWriterOnSerialCommunication::WithStorage<8192> writer{ serial };
            infra::TextOutputStream::WithErrorPolicy stream{ writer };
            services::TracerWithDateTime tracer{ stream };
            services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<> terminal{ serial, tracer };
        };

        struct Leds
        {
            StatePin operational;
            StatePin warning;
            StatePin failure;
        };

        void ControlTick();

        BoardOptions options;
        Execution execution;
        Console console;
        Leds leds;
        sil::PlantDrive drive;
        const foc::ThreePhaseMotorModel& plantModel{ drive.Model() };
        const foc::SimulatedEncoder& plantEncoder{ drive.Model().Encoder() };
        application::BoardProtectionSimulator boardProtection;
        FileEeprom eeprom;
        VirtualCanPort canPort;
        infra::ExecutionTimeStatistics controlLoopMetrics;
        application::PlatformDiagnostics diagnostics{ controlLoopMetrics };
        infra::Function<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
        infra::Function<void()> eachMillisecond;
        bool controlLoopEntered{ false };
    };
}
