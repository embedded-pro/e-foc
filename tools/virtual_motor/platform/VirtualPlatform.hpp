#pragma once

#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "infra/timer/TickOnInterruptTimerService.hpp"
#include "infra/util/ExecutionTimeStatistics.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"
#include "services/util/Terminal.hpp"
#include "tools/virtual_motor/platform/BoardPeripherals.hpp"
#include "tools/virtual_motor/platform/FileEeprom.hpp"
#include "tools/virtual_motor/platform/PlantInstruments.hpp"
#include "tools/virtual_motor/platform/PlantPowerStage.hpp"
#include "tools/virtual_motor/platform/RunLoop.hpp"
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
        std::string plantName;
    };

    class VirtualPlatform
        : public PlantPowerStage
        , public Stepper
    {
    public:
        explicit VirtualPlatform(const BoardOptions& options);

        void AdvanceOnePeriod() override;
        void AdvanceOneMillisecond() override;
        void Mute(bool muted) override;
        void EachMillisecond(const infra::Function<void()>& action);
        void EachIteration(const infra::Function<void()>& action);
        void MirrorLog(const infra::Function<void(infra::ConstByteRange text)>& sink);

        RunLoop& Runner();
        PlantInstruments& Instruments();
        const BoardOptions& Options() const;
        uint64_t ControlPeriods() const;
        uint8_t LedStates() const;

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
        foc::LowPriorityInterrupt& LowPriorityInterrupt() override;
        hal::Eeprom& Eeprom() override;
        application::PlatformDiagnostics& Diagnostics() override;
        void Reset() override;
        application::ResetCause GetResetCause() const override;
        infra::BoundedConstString FaultStatus() const override;

    private:
        void MillisecondTail();

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

        BoardOptions options;
        Execution execution;
        Console console;
        Leds leds;
        FileEeprom eeprom;
        VirtualCanPort canPort;
        infra::ExecutionTimeStatistics controlLoopMetrics;
        application::PlatformDiagnostics diagnostics{ controlLoopMetrics };
        infra::Function<void()> eachMillisecond;
        infra::Function<void()> eachIteration;
        RunLoop runLoop{ *this, []()
            {
                return Pacer::Clock::now();
            } };
        PlantInstruments instruments{ Disturbances() };
        uint64_t controlPeriods{ 0 };
        uint32_t periodInMillisecond{ 0 };
    };
}
