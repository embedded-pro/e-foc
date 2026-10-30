#include "targets/platform_implementations/qemu/implementation/PlatformFactoryImpl.hpp"
#include "core/platform_abstraction/LowSideShuntWindow.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingFile.hpp"
#include "targets/platform_implementations/qemu/implementation/SilPlantModel.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace
{
    constexpr int kResponseLinesPerDrain = 16;

    // No reset-cause register and reset reloads RAM, so the watchdog leaves its record in a host file.
    constexpr const char* watchdogResetMarkerPath = "watchdog_reset.bin";
    constexpr uint8_t watchdogResetMarker = 0x01;

    void WriteWatchdogResetMarker(uint8_t value)
    {
        const int handle = application::semihosting::FileOpen(watchdogResetMarkerPath, application::semihosting::OpenMode::writeBinary);
        if (handle < 0)
            return;

        application::semihosting::FileWrite(handle, &value, 1);
        application::semihosting::FileClose(handle);
    }

    application::ResetCause TakeWatchdogResetMarker()
    {
        const int handle = application::semihosting::FileOpen(watchdogResetMarkerPath, application::semihosting::OpenMode::readBinary);
        if (handle < 0)
            return application::ResetCause::powerUp;

        uint8_t value{ 0 };
        const bool read = application::semihosting::FileRead(handle, &value, 1) == 0;
        application::semihosting::FileClose(handle);

        if (!read || value != watchdogResetMarker)
            return application::ResetCause::powerUp;

        WriteWatchdogResetMarker(0);
        return application::ResetCause::watchdog;
    }

    long Milli(float value)
    {
        return std::lround(value * 1000.0f);
    }

    long Micro(float value)
    {
        return std::lround(value * 1000000.0f);
    }

    template<std::size_t Size>
    std::string_view Formatted(const std::array<char, Size>& buffer, int length)
    {
        return std::string_view{ buffer.data(), length > 0 ? std::min(static_cast<std::size_t>(length), Size - 1) : std::size_t{ 0 } };
    }
}

namespace application
{
    foc::ThreePhaseMotorModel::Parameters PlatformFactoryImpl::MotorParametersFrom(
        const std::optional<sil::SilPlantConfig>& config,
        const foc::ThreePhaseMotorModel::Parameters& fallback)
    {
        if (!config.has_value())
            return fallback;

        return sil::MotorParameters(*config);
    }

    foc::Volts PlatformFactoryImpl::SupplyVoltageFrom(const std::optional<sil::SilPlantConfig>& config)
    {
        return config.has_value() ? foc::Volts{ config->powerSupplyVoltageVolts } : foc::Volts{ kDefaultSupplyVoltageVolts };
    }

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

        sil::ConfigureModel(model, *plantConfig);
        bootFaultInjection = sil::BootFaultInjection(*plantConfig);
        ApplyCurrentSense(SampleAndHold::shortest);
        if (plantConfig->maxReadableDutyPercent != 0)
            maxReadableDuty = hal::DutyCycle::FromPercent(plantConfig->maxReadableDutyPercent);
        if (plantConfig->maxDutyPercent != 0)
            maxDuty = hal::DutyCycle::FromPercent(plantConfig->maxDutyPercent);
        shuntReadout = foc::LowSideShuntReadout{ maxReadableDuty };
        dutyDelayed = plantConfig->dutyDelayPeriods != 0;

        boardProtection.Configure({
            .overCurrentAmpere = plantConfig->overCurrentTripAmpere,
            .overVoltageVolts = plantConfig->overVoltageTripVolts,
            .underVoltageVolts = plantConfig->underVoltageTripVolts,
            .overTemperatureCelsius = plantConfig->overTemperatureTripCelsius,
        });

        const uint32_t baseHz = BaseFrequencyFrom(plantConfig).Value();

        if (plantConfig->responseSampleRateHz != 0)
        {
            really_assert(baseHz % plantConfig->responseSampleRateHz == 0);
            responseRecorder.Configure(baseHz / plantConfig->responseSampleRateHz, plantConfig->responseMaxSamples);
        }

        torqueStepTorque = foc::NewtonMeter{ plantConfig->torqueStepNm };
        const auto torqueStepDelay = static_cast<uint32_t>(static_cast<uint64_t>(plantConfig->torqueStepDelayMs) * baseHz / 1000u);
        torqueStep.Configure(plantConfig->torqueStepNm != 0.0f ? std::make_optional(torqueStepDelay) : std::nullopt);

        // The delay is carried in centiseconds because it reuses a byte the record already had.
        const auto encoderFreezeDelay = static_cast<uint32_t>(static_cast<uint64_t>(plantConfig->encoderFreezeDelayCentiseconds) * 10u * baseHz / 1000u);
        encoderFreeze.Configure(encoderFreezeDelay != 0 ? std::make_optional(encoderFreezeDelay) : std::nullopt);

        ConfigureShaftGrip(baseHz);

        if (plantConfig->eventLoopStallDelayMs != 0 && resetCause != ResetCause::watchdog)
            watchdogStall.Schedule(std::chrono::milliseconds(plantConfig->eventLoopStallDelayMs));
    }

    void PlatformFactoryImpl::ConfigureShaftGrip(uint32_t baseHz)
    {
        const auto ticks = [baseHz](uint32_t milliseconds)
        {
            return static_cast<uint32_t>(static_cast<uint64_t>(milliseconds) * baseHz / 1000u);
        };

        const bool gripped = plantConfig->shaftTurnRadians != 0.0f;
        const auto turnMs = std::max(plantConfig->shaftTurnMs, uint32_t{ 1 });
        shaftTurnSpeed = foc::RadiansPerSecond{ plantConfig->shaftTurnRadians * 1000.0f / static_cast<float>(turnMs) };
        shaftGrip.Configure(gripped ? std::make_optional(ticks(plantConfig->shaftGripDelayMs)) : std::nullopt);
        shaftHold.Configure(gripped ? std::make_optional(ticks(plantConfig->shaftGripDelayMs + turnMs)) : std::nullopt);
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
        , model(MotorParametersFrom(plantConfig, motorParams), SupplyVoltageFrom(plantConfig), BaseFrequencyFrom(plantConfig), std::nullopt, false)
        , baseFrequency(BaseFrequencyFrom(plantConfig))
    {
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

    hal::Hertz PlatformFactoryImpl::SystemClock() const
    {
        return hal::Hertz{ kQemuSystemClockHz };
    }

    foc::Volts PlatformFactoryImpl::PowerSupplyVoltage()
    {
        return model.EffectiveSupplyVoltage();
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
    {}

    ResetCause PlatformFactoryImpl::GetResetCause() const
    {
        return resetCause;
    }

    void PlatformFactoryImpl::Cortex::OnWatchdogExpired()
    {
        // The simulated bridge has no gate to cut; the machine reset that follows restarts the plant as well
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

    // Sample-and-hold follows the firmware's request, as on TI, unless a scenario pins it in the plant.
    void PlatformFactoryImpl::ApplyCurrentSense(SampleAndHold requested)
    {
        if (!plantConfig.has_value())
            return;

        static constexpr std::array<uint32_t, 5> sampleHoldClocks{ 4, 16, 32, 64, 256 };

        model.SetCurrentSense(sil::CurrentSense(*plantConfig, sampleHoldClocks.at(static_cast<std::size_t>(requested))));
    }

    void PlatformFactoryImpl::ConfigureAdcAndPwm(hal::Hertz, std::chrono::nanoseconds, SampleAndHold sampleAndHold)
    {
        ApplyCurrentSense(sampleAndHold);

        const auto freq = baseFrequency;

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

    CanBusAdapter& PlatformFactoryImpl::CanBus()
    {
        return *canBusAdapter;
    }

    void PlatformFactoryImpl::PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents)>& onDone)
    {
        onPhaseCurrentsReady.Set(onDone);
    }

    void PlatformFactoryImpl::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        lastDutyPhases = foc::KeepLowSideShuntsReadable(dutyPhases, maxReadableDuty, maxDuty);
    }

    void PlatformFactoryImpl::FocTimerIsr()
    {
        const uint32_t tick = controlTick.load(std::memory_order_relaxed) + 1;
        controlTick.store(tick, std::memory_order_relaxed);

        if (torqueStep.Fire(tick))
        {
            model.SetExternalTorque(torqueStepTorque);
            responseRecorder.Note(foc::PlantResponseKind::torqueStep, PlantSampleAt(tick));
        }

        if (encoderFreeze.Fire(tick))
        {
            auto frozen = bootFaultInjection;
            frozen.encoderStuck = true;
            model.SetFaultInjection(frozen);
            responseRecorder.Note(foc::PlantResponseKind::encoderFreeze, PlantSampleAt(tick));
        }

        if (shaftGrip.Fire(tick))
            model.GripShaft(shaftTurnSpeed);

        if (shaftHold.Fire(tick))
            model.GripShaft(foc::RadiansPerSecond{ 0.0f });

        const auto& stepDuty = dutyDelayed ? appliedDutyPhases : lastDutyPhases;
        model.StepForTest(stepDuty);
        shuntReadout.Applied(stepDuty);
        appliedDutyPhases = lastDutyPhases;
        responseRecorder.Capture(PlantSampleAt(tick));

        lastCurrents = model.LastMeasuredCurrents();

        if (onPhaseCurrentsReady.IsSet() && !controlLoopEntered)
        {
            controlLoopEntered = true;
            const auto entryCycles = QemuCycleCounter::Now();

            onPhaseCurrentsReady(shuntReadout.Read(lastCurrents));

            controlLoopMetrics.Record(QemuCycleCounter::Now() - entryCycles);
            controlLoopEntered = false;
        }
        else if (onPhaseCurrentsReady.IsSet() && controlLoopEntered)
            controlLoopMetrics.RecordReentry();

        boardProtection.Evaluate(lastCurrents, model.EffectiveSupplyVoltage().Value(), model.WindingTemperatureCelsius());
    }

    void PlatformFactoryImpl::Start()
    {
        model.Start();
        responseRecorder.Begin();
        torqueStep.Begin();
        encoderFreeze.Begin();
        shaftGrip.Begin();
        shaftHold.Begin();
        boardProtection.SetArmed(true);
    }

    void PlatformFactoryImpl::Stop()
    {
        boardProtection.SetArmed(false);
        model.Stop();
        model.SetExternalTorque(foc::NewtonMeter{ 0.0f });
        model.GripShaft(std::nullopt);
        model.SetFaultInjection(bootFaultInjection);
        responseRecorder.End();
    }

    foc::PlantResponseSample PlatformFactoryImpl::PlantSampleAt(uint32_t tick) const
    {
        const auto dq = model.LastDqCurrents();
        return foc::PlantResponseSample{ tick, model.MechanicalSpeed().Value(), model.MechanicalAngle().Value(), dq.q, dq.d, model.ExternalTorque().Value() };
    }

    void PlatformFactoryImpl::PollPorts()
    {
        if (const char* line = terminalPort.PollLine(); line != nullptr)
            serial.Deliver(line);

        if (!canBusAdapter)
            return;

        // Read before dispatch: the stamp is the response window's onset, so it must not fall after the setpoint took effect.
        const uint32_t tick = controlTick.load(std::memory_order_relaxed);
        if (const auto frame = canBusAdapter->PollIncoming())
            StampReceivedFrame(tick, *frame);
    }

    void PlatformFactoryImpl::DrainResponseRecords()
    {
        for (int i = 0; i != kResponseLinesPerDrain; ++i)
        {
            const auto record = responseRecorder.Pop();
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
        return baseFrequency;
    }

    foc::Ampere PlatformFactoryImpl::MaxCurrentSupported() const
    {
        return model.MaxCurrentSupported();
    }

    drivers::DeadTimeLoss PlatformFactoryImpl::InverterDeadTimeLoss([[maybe_unused]] std::chrono::nanoseconds deadTime) const
    {
        if (!plantConfig)
            return {};

        return { std::chrono::nanoseconds{ static_cast<int64_t>(plantConfig->deadTimeCompensationNanoseconds + 0.5f) }, foc::Ampere{ plantConfig->deadTimeCompensationKneeAmpere } };
    }

    uint32_t PlatformFactoryImpl::DutyUpdateDelayPeriods() const
    {
        return dutyDelayed ? 1 : 0;
    }

    foc::Radians PlatformFactoryImpl::Read()
    {
        return model.Read();
    }

    void PlatformFactoryImpl::Set(foc::Radians value)
    {
        model.Set(value);
    }

    void PlatformFactoryImpl::SetZero()
    {
        model.SetZero();
    }
}
