#include "targets/platform_implementations/qemu/implementation/PlatformFactoryImpl.hpp"
#include "bootloader/ImageFormat.hpp"
#include "generated/Boards.hpp"
#include "hal/cortex_m/Semihosting.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/math/Math.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string_view>

extern "C"
{
    extern const uint8_t __application_slot_start[];
    extern const uint8_t __application_slot_end[];
    extern const uint8_t __staging_slot_start[];
    extern const uint8_t __staging_slot_end[];
    extern const uint8_t __status_journal_start[];
    extern const uint8_t __status_journal_end[];
}

namespace
{
    constexpr int kResponseLinesPerDrain = 16;
    constexpr uint32_t stagingEraseUnit{ 16 * 1024 };
    constexpr uint16_t stagingPageSize{ 256 };
    constexpr uintptr_t aircrAddress{ 0xE000ED0Cu };
    constexpr uint32_t systemResetRequest{ 0x05FA0004u };

    std::array<uint8_t, 320 * 1024> stagingStorage;
    std::array<uint8_t, 16 * 1024> journalStorage;

    constexpr const char* watchdogResetMarkerPath = "watchdog_reset.bin";
    constexpr uint8_t watchdogResetMarker = 0x01;

    void WriteWatchdogResetMarker(uint8_t value)
    {
        const auto handle = hal::cortex::SemihostingOpenFile(watchdogResetMarkerPath, hal::cortex::SemihostingOpenMode::writeBinary);
        if (handle < 0)
            return;

        hal::cortex::SemihostingWriteFile(handle, infra::MakeConstByteRange(value));
        hal::cortex::SemihostingCloseFile(handle);
    }

    application::ResetCause TakeWatchdogResetMarker()
    {
        const auto handle = hal::cortex::SemihostingOpenFile(watchdogResetMarkerPath, hal::cortex::SemihostingOpenMode::readBinary);
        if (handle < 0)
            return application::ResetCause::powerUp;

        uint8_t value{ 0 };
        const bool read = hal::cortex::SemihostingReadFile(handle, infra::MakeByteRange(value)) == 0;
        hal::cortex::SemihostingCloseFile(handle);

        if (!read || value != watchdogResetMarker)
            return application::ResetCause::powerUp;

        WriteWatchdogResetMarker(0);
        return application::ResetCause::watchdog;
    }

    long Milli(float value)
    {
        return static_cast<long>(math::Round(value * 1000.0f));
    }

    long Micro(float value)
    {
        return static_cast<long>(math::Round(value * 1000000.0f));
    }

    template<std::size_t Size>
    std::string_view Formatted(const std::array<char, Size>& buffer, int length)
    {
        return std::string_view{ buffer.data(), length > 0 ? std::min(static_cast<std::size_t>(length), Size - 1) : std::size_t{ 0 } };
    }
}

namespace application
{
    hal::Hertz PlatformFactoryImpl::BaseFrequencyFrom(const std::optional<sil::SilPlantConfig>& config)
    {
        if (!config.has_value() || config->baseFrequencyHz == 0)
            return hal::Hertz{ kDefaultBaseFrequencyHz };

        return hal::Hertz{ config->baseFrequencyHz };
    }

    void PlatformFactoryImpl::ApplyPlantConfig()
    {
        if (!plantConfig.has_value())
            return;

        boardProtection.Configure({
            .overCurrentAmpere = plantConfig->overCurrentTripAmpere,
            .overVoltageVolts = plantConfig->overVoltageTripVolts,
            .underVoltageVolts = plantConfig->underVoltageTripVolts,
            .overTemperatureCelsius = plantConfig->overTemperatureTripCelsius,
        });

        if (plantConfig->eventLoopStallDelayMs != 0 && resetCause != ResetCause::watchdog)
            watchdogStall.Schedule(std::chrono::milliseconds(plantConfig->eventLoopStallDelayMs));
    }

    PlatformFactoryImpl::PlatformFactoryImpl(const foc::ThreePhaseMotorModel::Parameters& motorParams,
        const infra::Function<void()>& onInit)
        : plantConfig(LoadSilPlantConfig(sil::plantConfigFileName))
        , resetCause(TakeWatchdogResetMarker())
        , onInitialized(onInit)
        , focTimer(0x40000000u, 8, kQemuSystemClockHz, BaseFrequencyFrom(plantConfig).Value(), [this]()
              {
                  FocTimerIsr();
              })
        , stagingFlash(fileSystem, "staging.bin", infra::MakeRange(stagingStorage), stagingEraseUnit)
        , journalFlash(fileSystem, "journal.bin", infra::MakeRange(journalStorage), stagingEraseUnit)
        , drive(plantConfig, sil::PlantDriveDefaults{ motorParams, foc::Volts{ kDefaultSupplyVoltageVolts }, hal::Hertz{ kDefaultBaseFrequencyHz } })
    {
        really_assert(stagingStorage.size() == static_cast<std::size_t>(__staging_slot_end - __staging_slot_start) && journalStorage.size() == static_cast<std::size_t>(__status_journal_end - __status_journal_start));
        services::SetGlobalTracerInstance(terminalAndTracer.tracer);
        ApplyPlantConfig();
        onInitialized();
        protectionPollTimer.Start(std::chrono::milliseconds(1), [this]()
            {
                boardProtection.DeliverPendingProtection();
            });
        responseDrainTimer.Start(std::chrono::milliseconds(1), [this]()
            {
                DrainResponseRecords();
            });
        portPollTimer.Start(std::chrono::milliseconds(1), [this]()
            {
                PollPorts();
            });
        focTimer.Start();
    }

    void PlatformFactoryImpl::Run()
    {
        std::puts("\nREADY");
        std::fflush(stdout);

        while (true)
            cortex.eventDispatcher.ExecuteAllActions();
    }

    services::Tracer& PlatformFactoryImpl::Tracer()
    {
        return terminalAndTracer.tracer;
    }

    services::TerminalWithCommands& PlatformFactoryImpl::Terminal()
    {
        return terminalAndTracer.terminal;
    }

    hal::GpioPin& PlatformFactoryImpl::OperationalLed()
    {
        return operationalPin;
    }

    hal::GpioPin& PlatformFactoryImpl::WarningLed()
    {
        return warningPin;
    }

    hal::GpioPin& PlatformFactoryImpl::FailureLed()
    {
        return failurePin;
    }

    uint8_t PlatformFactoryImpl::BoardId() const
    {
        return 0;
    }

    BoardKind PlatformFactoryImpl::GetBoardKind() const
    {
        return static_cast<BoardKind>(e_foc::board.kind);
    }

    hal::Hertz PlatformFactoryImpl::SystemClock() const
    {
        return hal::Hertz{ kQemuSystemClockHz };
    }

    foc::Volts PlatformFactoryImpl::PowerSupplyVoltage()
    {
        return drive.Model().Supply().Voltage();
    }

    foc::LowPriorityInterrupt& PlatformFactoryImpl::LowPriorityInterrupt()
    {
        return pendSvLowPriorityInterrupt;
    }

    hal::Eeprom& PlatformFactoryImpl::Eeprom()
    {
        return eeprom;
    }

    void PlatformFactoryImpl::RegisterBoardProtection(const infra::Function<void(BoardProtectionReason)>& onProtection)
    {
        boardProtection.Register(onProtection);
    }

    PlatformFactory::BoardProtectionState PlatformFactoryImpl::BoardProtectionStatus()
    {
        return boardProtection.Status();
    }

    void PlatformFactoryImpl::Reset()
    {
        __asm volatile("dsb" ::: "memory");
        *reinterpret_cast<volatile uint32_t*>(aircrAddress) = systemResetRequest;
        __asm volatile("dsb" ::: "memory");

        for (;;)
        {
        }
    }

    std::optional<services::FirmwareStaging> PlatformFactoryImpl::Staging()
    {
        const auto slotSize = static_cast<uint32_t>(__application_slot_end - __application_slot_start);
        return services::FirmwareStaging{ stagingFlash, journalFlash, e_foc::board.identification, slotSize - bootloader::imageDescriptorSize, stagingPageSize };
    }

    ResetCause PlatformFactoryImpl::GetResetCause() const
    {
        return resetCause;
    }

    void PlatformFactoryImpl::Cortex::OnWatchdogExpired()
    {
        WriteWatchdogResetMarker(watchdogResetMarker);
    }

    infra::BoundedConstString PlatformFactoryImpl::FaultStatus() const
    {
        return {};
    }

    PlatformDiagnostics& PlatformFactoryImpl::Diagnostics()
    {
        return diagnostics;
    }

    void PlatformFactoryImpl::ConfigureAdcAndPwm(hal::Hertz, std::chrono::nanoseconds, SampleAndHold sampleAndHold)
    {
        static constexpr std::array<uint32_t, 5> sampleHoldClocks{ 4, 16, 32, 64, 256 };
        drive.SampleAndHoldClocks(sampleHoldClocks.at(static_cast<std::size_t>(sampleAndHold)));

        const auto freq = drive.BaseFrequency();

        const auto periodCycles = freq.Value() == 0
                                      ? 0u
                                      : static_cast<uint32_t>(kQemuSystemClockHz / freq.Value());
        controlLoopMetrics.Configure(static_cast<uint32_t>(static_cast<uint64_t>(periodCycles) * 3u / 4u), periodCycles);
    }

    void PlatformFactoryImpl::SetEncoderResolution(uint32_t)
    {}

    void PlatformFactoryImpl::ConfigureCanBus(uint32_t, bool)
    {
        if (!canBusAdapter)
        {
            canBusAdapter.emplace(kCanUartBase);
            diagnostics.AttachCanBus(*canBusAdapter);
        }
    }

    services::MonitoredCan& PlatformFactoryImpl::CanBus()
    {
        return *canBusAdapter;
    }

    void PlatformFactoryImpl::PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents)>& onDone)
    {
        onPhaseCurrentsReady.Set(onDone);
    }

    void PlatformFactoryImpl::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        drive.Output(dutyPhases);
    }

    void PlatformFactoryImpl::FocTimerIsr()
    {
        drive.Tick();

        if (onPhaseCurrentsReady.IsSet() && !controlLoopEntered)
        {
            controlLoopEntered = true;
            const auto entryCycles = QemuCycleCounter::Now();

            onPhaseCurrentsReady(drive.ShuntCurrents());

            controlLoopMetrics.Record(QemuCycleCounter::Now() - entryCycles);
            controlLoopEntered = false;
        }
        else if (onPhaseCurrentsReady.IsSet() && controlLoopEntered)
            controlLoopMetrics.RecordReentry();

        auto& model = drive.Model();
        boardProtection.Evaluate(model.CurrentSensor().Last(), model.Supply().Voltage().Value(), model.Thermal().Temperature());
    }

    void PlatformFactoryImpl::Start()
    {
        drive.Start();
        boardProtection.SetArmed(true);
    }

    void PlatformFactoryImpl::Stop()
    {
        boardProtection.SetArmed(false);
        drive.Stop();
    }

    void PlatformFactoryImpl::PollPorts()
    {
        if (const char* line = terminalPort.PollLine(); line != nullptr)
            serial.Deliver(line);

        if (!canBusAdapter)
            return;

        const uint32_t tick = drive.TickCount();
        if (const auto frame = canBusAdapter->PollIncoming())
            StampReceivedFrame(tick, *frame);
    }

    void PlatformFactoryImpl::DrainResponseRecords()
    {
        for (int i = 0; i != kResponseLinesPerDrain; ++i)
        {
            const auto record = drive.PopRecord();
            if (!record)
                break;

            RecordResponse(*record);
        }

        plantRecord.Flush();
    }

    void PlatformFactoryImpl::RecordResponse(const foc::PlantResponseRecord& record)
    {
        const auto& sample = record.sample;
        const auto tick = static_cast<unsigned long>(sample.tick);
        std::array<char, 96> line{};
        int length = 0;

        switch (record.kind)
        {
            case foc::PlantResponseKind::sample:
                length = std::snprintf(line.data(), line.size(), "PLANT %lu %ld %ld %ld %ld %ld\n", tick, Milli(sample.omegaMech), Micro(sample.thetaMech), Micro(sample.iq), Micro(sample.id), Micro(sample.externalTorqueNm));
                break;
            case foc::PlantResponseKind::began:
                length = std::snprintf(line.data(), line.size(), "PLANT_START %lu\n", tick);
                break;
            case foc::PlantResponseKind::torqueStep:
                length = std::snprintf(line.data(), line.size(), "PLANT_EVENT %lu torque %ld\n", tick, Micro(sample.externalTorqueNm));
                break;
            case foc::PlantResponseKind::encoderFreeze:
                length = std::snprintf(line.data(), line.size(), "PLANT_EVENT %lu encoder_freeze %ld\n", tick, Micro(sample.thetaMech));
                break;
            case foc::PlantResponseKind::stopped:
                length = std::snprintf(line.data(), line.size(), "PLANT_STOP %lu %lu\n", tick, static_cast<unsigned long>(record.dropped));
                break;
        }

        plantRecord.Append(Formatted(line, length));
    }

    void PlatformFactoryImpl::StampReceivedFrame(uint32_t tick, const sil::EmulatedCan::Frame& frame)
    {
        char hexData[17]{};
        int pos = 0;
        for (const uint8_t byte : frame.message)
        {
            hexData[pos++] = "0123456789abcdef"[(byte >> 4) & 0xF];
            hexData[pos++] = "0123456789abcdef"[byte & 0xF];
        }
        hexData[pos] = '\0';

        const uint32_t rawId = frame.id.Is11BitId() ? frame.id.Get11BitId() : frame.id.Get29BitId();
        std::array<char, 64> line{};
        const int length = std::snprintf(line.data(), line.size(), "CAN_RX_AT %lu %lx %s\n", static_cast<unsigned long>(tick), static_cast<unsigned long>(rawId), hexData);
        plantRecord.Append(Formatted(line, length));
        plantRecord.Flush();
    }

    hal::Hertz PlatformFactoryImpl::BaseFrequency() const
    {
        return drive.BaseFrequency();
    }

    foc::Ampere PlatformFactoryImpl::MaxCurrentSupported() const
    {
        return plantModel.MaxCurrentSupported();
    }

    void PlatformFactoryImpl::CurrentOffsets(const foc::PhaseCurrents& offsets)
    {
        drive.CurrentOffsets(offsets);
    }

    void PlatformFactoryImpl::ZeroVectorSlopes(const foc::PhaseCurrentSlopes& slopes)
    {
        drive.ZeroVectorSlopes(slopes);
    }

    drivers::DeadTimeLoss PlatformFactoryImpl::InverterDeadTimeLoss([[maybe_unused]] std::chrono::nanoseconds deadTime) const
    {
        return drive.InverterDeadTimeLoss();
    }

    uint32_t PlatformFactoryImpl::DutyUpdateDelayPeriods() const
    {
        return drive.DutyUpdateDelayPeriods();
    }

    foc::Radians PlatformFactoryImpl::Read()
    {
        return drive.Model().Encoder().Read();
    }

    void PlatformFactoryImpl::Set(foc::Radians value)
    {
        drive.Model().Encoder().Set(value);
    }

    void PlatformFactoryImpl::SetZero()
    {
        drive.Model().Encoder().SetZero();
    }

    std::optional<std::reference_wrapper<drivers::EncoderIndex>> PlatformFactoryImpl::EncoderIndexChannel()
    {
        return *this;
    }

    bool PlatformFactoryImpl::HasIndex() const
    {
        return plantEncoder.HasIndex();
    }

    void PlatformFactoryImpl::ArmIndex()
    {
        drive.ArmIndex();
    }

    bool PlatformFactoryImpl::IndexSeen()
    {
        return drive.IndexSeen();
    }
}
