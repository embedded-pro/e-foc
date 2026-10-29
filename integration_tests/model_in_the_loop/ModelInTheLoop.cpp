#include "integration_tests/model_in_the_loop/ModelInTheLoop.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>
#include <cstdio>
#include <memory>

namespace mil
{
    namespace
    {
        constexpr float kMilliPerUnit = 1000.0f;
        constexpr float kMilliPerSecond = 1000.0f;
        constexpr std::size_t kTracerTailCharacters = 4000;
        constexpr auto kBootTime = std::chrono::milliseconds{ 10 };
        constexpr auto kCommandTimeout = std::chrono::seconds{ 30 };
        constexpr auto kCalibrationTimeout = std::chrono::seconds{ 180 };
        constexpr auto kStateTimeout = std::chrono::seconds{ 1 };
        constexpr auto kEnableObservation = std::chrono::milliseconds{ 500 };
        constexpr auto kCommandSpacing = std::chrono::milliseconds{ 5 };

        bool ScaleStoredCalibration(services::CalibrationData& calibration, const std::string& quantity, float factor)
        {
            if (quantity == "resistance")
                calibration.rPhase *= factor;
            else if (quantity == "inductance")
            {
                calibration.lD *= factor;
                calibration.lQ *= factor;
            }
            else if (quantity == "inertia")
                calibration.inertia *= factor;
            else if (quantity == "friction")
                calibration.frictionViscous *= factor;
            else
                return false;

            return true;
        }

        template<class T>
        bool Holds(const state_machine::State& state)
        {
            return std::holds_alternative<T>(state);
        }
    }

    Scenario& Scenario::Plant(const std::string& preset)
    {
        really_assert(sil::TryNamedPlant(preset, plant));
        plantName = preset;
        return *this;
    }

    Scenario& Scenario::Calibrated()
    {
        nvm.includeCalibration = true;
        nvm.includeConfig = true;
        nvm.calibration = sil::CompleteCalibration();
        nvm.calibration.polePairs = plant.polePairs;
        nvm.calibration.rPhase = plant.statorResistanceOhm;
        nvm.calibration.lD = plant.dAxisInductanceHenry * kMilliPerUnit;
        nvm.calibration.lQ = plant.qAxisInductanceHenry * kMilliPerUnit;
        nvm.calibration.fluxLinkage = plant.fluxLinkageWeber;
        nvm.calibration.inertia = plant.rotorInertiaKgM2;
        nvm.calibration.frictionViscous = plant.viscousDampingNmSPerRad;
        nvm.calibration.currentOffsetA = plant.noiseBiasAmpereA;
        nvm.calibration.currentOffsetB = plant.noiseBiasAmpereB;
        nvm.calibration.currentOffsetC = plant.noiseBiasAmpereC;
        return *this;
    }

    Scenario& Scenario::StoredCalibrationOffBy(const std::string& quantity, float factor)
    {
        really_assert(nvm.includeCalibration);
        really_assert(ScaleStoredCalibration(nvm.calibration, quantity, factor));
        return *this;
    }

    Scenario& Scenario::BootsIn(state_machine::ControlMode mode)
    {
        nvm.includeConfig = true;
        nvm.config.defaultControlMode = static_cast<uint8_t>(mode);
        return *this;
    }

    Scenario& Scenario::CurrentLoop(foc::CurrentAlgorithm algorithm)
    {
        nvm.includeConfig = true;
        nvm.config.currentAlgorithm = static_cast<uint8_t>(algorithm);
        return *this;
    }

    Scenario& Scenario::SpeedLoop(foc::SpeedAlgorithm algorithm)
    {
        nvm.includeConfig = true;
        nvm.config.speedAlgorithm = static_cast<uint8_t>(algorithm);
        return *this;
    }

    Scenario& Scenario::PositionLoop(foc::PositionAlgorithm algorithm)
    {
        nvm.includeConfig = true;
        nvm.config.positionAlgorithm = static_cast<uint8_t>(algorithm);
        return *this;
    }

    Scenario& Scenario::Recorded(uint32_t sampleRateHz, uint32_t maxSamples)
    {
        really_assert(sampleRateHz != 0 && plant.baseFrequencyHz % sampleRateHz == 0);
        plant.responseSampleRateHz = sampleRateHz;
        plant.responseMaxSamples = maxSamples;
        return *this;
    }

    Scenario& Scenario::TorqueStep(float torqueNm, uint32_t delayMs)
    {
        plant.torqueStepNm = torqueNm;
        plant.torqueStepDelayMs = delayMs;
        return *this;
    }

    void ModelInTheLoop::DeferredLowPriorityInterrupt::Trigger()
    {
        pending = true;
    }

    void ModelInTheLoop::DeferredLowPriorityInterrupt::Register(const infra::Function<void()>& handler)
    {
        this->handler = handler;
    }

    void ModelInTheLoop::DeferredLowPriorityInterrupt::Unregister()
    {
        handler = nullptr;
        pending = false;
    }

    void ModelInTheLoop::DeferredLowPriorityInterrupt::RunPending()
    {
        if (!pending)
            return;

        pending = false;
        if (handler != nullptr)
            handler();
    }

    void ModelInTheLoop::SilentSerial::SendData(infra::ConstByteRange, infra::Function<void()> actionOnCompletion)
    {
        infra::EventDispatcher::Instance().Schedule(actionOnCompletion);
    }

    void ModelInTheLoop::SilentSerial::ReceiveData(infra::Function<void(infra::ConstByteRange data)>)
    {}

    ModelInTheLoop::MemoryEeprom::MemoryEeprom(std::vector<uint8_t> contents)
        : contents{ std::move(contents) }
    {}

    uint32_t ModelInTheLoop::MemoryEeprom::Size() const
    {
        return static_cast<uint32_t>(contents.size());
    }

    void ModelInTheLoop::MemoryEeprom::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        really_assert(address + buffer.size() <= contents.size());
        std::copy(buffer.begin(), buffer.end(), contents.begin() + address);
        infra::EventDispatcher::Instance().Schedule(onDone);
    }

    void ModelInTheLoop::MemoryEeprom::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        really_assert(address + buffer.size() <= contents.size());
        std::copy(contents.begin() + address, contents.begin() + address + buffer.size(), buffer.begin());
        infra::EventDispatcher::Instance().Schedule(onDone);
    }

    void ModelInTheLoop::MemoryEeprom::Erase(infra::Function<void()> onDone)
    {
        std::fill(contents.begin(), contents.end(), 0xFF);
        infra::EventDispatcher::Instance().Schedule(onDone);
    }

    ModelInTheLoop::ModelInTheLoop(const Scenario& scenario)
        : scenario{ scenario }
        , drive{ scenario.plant }
        , inverter{ drive }
        , eeprom{ scenario.nvm.includeCalibration || scenario.nvm.includeConfig ? sil::BuildNvmImage(scenario.nvm) : sil::BlankNvmImage() }
        , vdc{ drive.PowerSupplyVoltage() }
        , tickPeriod{ std::chrono::nanoseconds{ 1000000000u / drive.BaseFrequency().Value() } }
        , ticksPerMillisecond{ drive.BaseFrequency().Value() / 1000u }
        , inverterCompensation{ inverter, drive.InverterDeadTimeLoss() }
        , currentOffsets{ inverter, inverterCompensation }
        , electricalIdent{ inverterCompensation, drive, vdc }
        , motorAlignment{ inverterCompensation, drive }
        , rotorStandstill{ drive }
    {
        nvm.LoadConfig(configData, [this](services::NvmStatus status)
            {
                if (status != services::NvmStatus::Ok)
                    configData = services::MakeDefaultConfigData();

                controlMode.emplace(
                    application::TerminalAndTracer{ terminal, tracer },
                    application::MotorHardware{ inverterCompensation, drive, vdc },
                    nvm,
                    application::CalibrationServices{ .electricalIdent = *electricalIdent, .motorAlignment = *motorAlignment, .fluxLinkage = foc::Weber{ motorFluxLinkageWb }, .agreeingMechanicalRuns = 2, .currentOffsets = *currentOffsets, .mapCogging = true, .standstill = rotorStandstill },
                    faultNotifier,
                    configData,
                    state_machine::ControlModeStateMachine::OuterLoopArgs{
                        .maxCurrent = drive.MaxCurrentSupported(),
                        .baseFrequency = drive.BaseFrequency(),
                        .lowPriorityInterrupt = lowPriorityInterrupt,
                        .dutyUpdateDelayPeriods = drive.DutyUpdateDelayPeriods() });
            });

        Run(kBootTime);
        really_assert(controlMode.has_value());
    }

    ModelInTheLoop::~ModelInTheLoop()
    {
        controlMode.reset();
        clock.ExecuteAllActions();
    }

    void ModelInTheLoop::Step()
    {
        drive.ControlTick();
        lowPriorityInterrupt.RunPending();
        clock.ForwardTime(tickPeriod);

        if (drive.Tick() % ticksPerMillisecond == 0)
            DrainRecords();
    }

    void ModelInTheLoop::DrainRecords()
    {
        recordLines.clear();
        drive.AppendRecords(recordLines, recordsPerDrain);
        for (const auto& line : recordLines)
            trace.Consume(line);
    }

    void ModelInTheLoop::Run(std::chrono::microseconds duration)
    {
        const auto ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(duration) / tickPeriod;
        for (auto i = decltype(ticks){ 0 }; i != ticks; ++i)
            Step();
    }

    bool ModelInTheLoop::RunUntil(const std::function<bool()>& done, std::chrono::microseconds limit)
    {
        const auto ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(limit) / tickPeriod;
        for (auto i = decltype(ticks){ 0 }; i != ticks && !done(); ++i)
            Step();

        return done();
    }

    bool ModelInTheLoop::Command(const std::function<void(const infra::Function<void(state_machine::CommandResult)>&)>& issue, std::chrono::microseconds limit)
    {
        auto result = std::make_shared<std::optional<state_machine::CommandResult>>();
        issue([result](state_machine::CommandResult outcome)
            {
                *result = outcome;
            });

        const bool completed = RunUntil([&result]()
            {
                return result->has_value();
            },
            limit);
        Run(kCommandSpacing);
        return completed && **result == state_machine::CommandResult::ok;
    }

    bool ModelInTheLoop::RunUntilState(const std::function<bool(const state_machine::State&)>& predicate, std::chrono::microseconds limit)
    {
        return RunUntil([this, &predicate]()
            {
                return predicate(controlMode->ActiveStateMachine().CurrentState());
            },
            limit);
    }

    bool ModelInTheLoop::Align()
    {
        return Command([this](const auto& onDone)
            {
                controlMode->CmdReAlign(onDone);
            },
            kCommandTimeout);
    }

    bool ModelInTheLoop::Calibrate()
    {
        return Command([this](const auto& onDone)
            {
                controlMode->ActiveStateMachine().CmdCalibrate(onDone);
            },
            kCalibrationTimeout);
    }

    bool ModelInTheLoop::ClearCalibration()
    {
        return Command([this](const auto& onDone)
            {
                controlMode->ActiveStateMachine().CmdClearCalibration(onDone);
            },
            kCommandTimeout);
    }

    std::optional<ElectricalIdentification> ModelInTheLoop::IdentifyElectrical()
    {
        if (controlMode->CmdReserveExternalCalibration() != state_machine::CommandResult::ok)
            return std::nullopt;

        using Result = services::ElectricalParametersIdentification::ResistanceInductanceResult;
        auto resistanceAndInductance = std::make_shared<std::optional<Result>>();
        electricalIdent->EstimateResistanceAndInductance({}, [resistanceAndInductance](Result result)
            {
                *resistanceAndInductance = result;
            });

        if (!RunUntil([&resistanceAndInductance]()
                {
                    return resistanceAndInductance->has_value();
                },
                kCalibrationTimeout))
            return std::nullopt;

        const auto& measured = **resistanceAndInductance;
        if (!measured.resistance.has_value() || !measured.inductance.has_value() || measured.fitQuality < 0.5f)
        {
            controlMode->CmdReleaseExternalCalibration();
            return std::nullopt;
        }

        auto polePairs = std::make_shared<std::optional<std::optional<std::size_t>>>();
        electricalIdent->EstimateNumberOfPolePairs({}, [polePairs](std::optional<std::size_t> estimate)
            {
                *polePairs = estimate;
            });

        if (!RunUntil([&polePairs]()
                {
                    return polePairs->has_value();
                },
                kCalibrationTimeout) ||
            !polePairs->value().has_value())
        {
            controlMode->CmdReleaseExternalCalibration();
            return std::nullopt;
        }

        services::CalibrationData data{};
        data.rPhase = measured.resistance->Value();
        data.lD = measured.inductance->Value();
        data.lQ = measured.inductance->Value();
        data.polePairs = static_cast<uint8_t>(*polePairs->value());
        data.fluxLinkage = controlMode->ActiveFluxLinkage().Value();

        if (!Command([this, &data](const auto& onDone)
                {
                    controlMode->CmdCompleteExternalCalibration(data, onDone);
                },
                kCommandTimeout))
            return std::nullopt;

        return ElectricalIdentification{ data.rPhase, data.lD, data.polePairs };
    }

    bool ModelInTheLoop::Enable()
    {
        if (controlMode->ActiveStateMachine().CmdEnable() != state_machine::CommandResult::ok)
            return false;

        if (!RunUntilState(Holds<state_machine::Enabled>, kStateTimeout))
            return false;

        Run(kEnableObservation);
        return IsRunning();
    }

    bool ModelInTheLoop::Disable()
    {
        if (controlMode->ActiveStateMachine().CmdDisable() != state_machine::CommandResult::ok)
            return false;

        const bool ready = RunUntilState(Holds<state_machine::Ready>, kStateTimeout);
        Run(kCommandSpacing);
        return ready;
    }

    bool ModelInTheLoop::Select(state_machine::ControlMode mode)
    {
        auto result = std::make_shared<std::optional<state_machine::SelectResult>>();
        controlMode->Select(mode, [result](state_machine::SelectResult outcome)
            {
                *result = outcome;
            });

        const bool completed = RunUntil([&result]()
            {
                return result->has_value();
            },
            kCommandTimeout);
        Run(kCommandSpacing);
        return completed && **result == state_machine::SelectResult::ok;
    }

    bool ModelInTheLoop::SetTorque(float ampere)
    {
        StampSetpoint(ampere);
        const bool accepted = controlMode->TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ ampere } });
        Run(kCommandSpacing);
        return accepted;
    }

    bool ModelInTheLoop::SetSpeed(float radiansPerSecond)
    {
        StampSetpoint(radiansPerSecond);
        const bool accepted = controlMode->TrySetSpeed(foc::RadiansPerSecond{ radiansPerSecond });
        Run(kCommandSpacing);
        return accepted;
    }

    bool ModelInTheLoop::SetPosition(float radians)
    {
        StampSetpoint(radians);
        const bool accepted = controlMode->TrySetPosition(foc::Radians{ radians });
        Run(kCommandSpacing);
        return accepted;
    }

    bool ModelInTheLoop::CaptureAfterEnable(float milliseconds)
    {
        const auto window = TicksFromMilliseconds(milliseconds);
        return RunUntil([this, window]()
            {
                const auto start = trace.StartTick();
                const auto last = trace.LastSampleTick();
                return start && ((last && *last >= *start + window) || trace.StopTick());
            },
            kCommandTimeout);
    }

    bool ModelInTheLoop::CaptureAfterSetpoint(float milliseconds)
    {
        const auto window = TicksFromMilliseconds(milliseconds);
        return RunUntil([this, window]()
            {
                const auto last = trace.LastSampleTick();
                return setpointTick && ((last && *last >= *setpointTick + window) || trace.StopTick());
            },
            kCommandTimeout);
    }

    bool ModelInTheLoop::AlternateSpeed(float low, float high, std::chrono::milliseconds dwell, std::chrono::milliseconds total)
    {
        const auto start = drive.Tick();
        const auto totalTicks = TicksFromMilliseconds(static_cast<float>(total.count()));
        bool atHigh = false;

        while (drive.Tick() < start + totalTicks)
        {
            atHigh = !atHigh;
            if (!SetSpeed(atHigh ? high : low))
                return false;
            Run(dwell);
        }

        return IsRunning();
    }

    bool ModelInTheLoop::HoldSpeed(float speed, std::chrono::milliseconds duration)
    {
        return AlternateSpeed(speed, speed, duration, duration);
    }

    void ModelInTheLoop::StampSetpoint(float value)
    {
        setpointTick = drive.Tick();
        setpoint = value;
    }

    bool ModelInTheLoop::IsRunning() const
    {
        return Holds<state_machine::Enabled>(controlMode->ActiveStateMachine().CurrentState());
    }

    bool ModelInTheLoop::HasFault() const
    {
        return Holds<state_machine::Fault>(controlMode->ActiveStateMachine().CurrentState());
    }

    bool ModelInTheLoop::IsReady() const
    {
        return Holds<state_machine::Ready>(controlMode->ActiveStateMachine().CurrentState());
    }

    state_machine::ControlModeStateMachine& ModelInTheLoop::ControlMode()
    {
        return *controlMode;
    }

    std::optional<state_machine::OnlineEstimates> ModelInTheLoop::OnlineEstimates()
    {
        return controlMode->ActiveStateMachine().CurrentOnlineEstimates();
    }

    const services::CalibrationData& ModelInTheLoop::CalibrationRecord() const
    {
        return controlMode->CalibrationRecord();
    }

    const Scenario& ModelInTheLoop::Setup() const
    {
        return scenario;
    }

    SimulatedDrive& ModelInTheLoop::Drive()
    {
        return drive;
    }

    const integration::response::PlantTrace& ModelInTheLoop::Trace()
    {
        return trace;
    }

    std::optional<uint32_t> ModelInTheLoop::SetpointTick() const
    {
        return setpointTick;
    }

    std::optional<float> ModelInTheLoop::Setpoint() const
    {
        return setpoint;
    }

    uint32_t ModelInTheLoop::TicksFromMilliseconds(float milliseconds) const
    {
        return static_cast<uint32_t>(milliseconds * static_cast<float>(drive.BaseFrequency().Value()) / kMilliPerSecond);
    }

    std::string ModelInTheLoop::TracerTail() const
    {
        const auto& log = tracerStream.Storage();
        return log.size() > kTracerTailCharacters ? log.substr(log.size() - kTracerTailCharacters) : log;
    }

    std::optional<integration::response::StepResponseMetrics> MeasureStep(ModelInTheLoop& mil, integration::response::Signal signal, float bandPercent, const char* label)
    {
        using namespace integration::response;

        const auto& trace = mil.Trace();
        if (!trace.StartTick() || !mil.Setpoint() || !trace.TickSpacing())
            return std::nullopt;

        const auto stamp = mil.SetpointTick();
        const bool whileRunning = stamp.has_value() && *stamp >= *trace.StartTick();
        const uint32_t onset = whileRunning ? *stamp : *trace.StartTick();
        const float initial = whileRunning ? ValueAt(trace, signal, onset).value_or(0.0f) : 0.0f;
        const float reference = *mil.Setpoint();
        const auto raw = Extract(trace, signal, onset, StepWindowSamples(signal));
        if (raw.size() < StepWindowSamples(signal))
            return std::nullopt;

        const float dt = static_cast<float>(*trace.TickSpacing()) / static_cast<float>(mil.Drive().BaseFrequency().Value());
        const auto metrics = ComputeStepMetricsFor(signal, NormaliseStep(raw, initial, reference), dt, bandPercent / 100.0f, reference - initial);
        if (!metrics)
            return std::nullopt;

        std::fprintf(stderr, "[METRIC] mil %s step %s %s from=%.4g to=%.4g band_pct=%.1f rise_ms=%.3f settle_ms=%.3f overshoot_pct=%.2f tail_band_pct=%.2f ss_err=%.4g %s\n",
            SignalName(signal), label, mil.Setup().plantName.c_str(), initial, reference, bandPercent,
            metrics->riseTimeS * kMilliPerSecond, metrics->settlingTimeS * kMilliPerSecond,
            metrics->percentOvershoot, metrics->tailBandPercent, metrics->steadyStateError, SignalUnit(signal));
        return metrics;
    }

    std::optional<integration::response::DisturbanceResponseMetrics> MeasureDisturbance(ModelInTheLoop& mil, integration::response::Signal signal, float absoluteBand)
    {
        using namespace integration::response;

        const auto& trace = mil.Trace();
        const auto onset = trace.EventTick("torque");
        if (!onset || !mil.Setpoint() || !trace.TickSpacing())
            return std::nullopt;

        const auto raw = Extract(trace, signal, *onset, disturbanceWindowSamples);
        if (raw.size() < disturbanceWindowSamples)
            return std::nullopt;

        const float dt = static_cast<float>(*trace.TickSpacing()) / static_cast<float>(mil.Drive().BaseFrequency().Value());
        const auto metrics = ComputeDisturbanceMetrics<disturbanceWindowSamples>(raw, *mil.Setpoint(), dt, absoluteBand);
        if (!metrics)
            return std::nullopt;

        std::fprintf(stderr, "[METRIC] mil %s disturbance %s reference=%.4g max_deviation=%.4g recovery_ms=%.3f ss_err=%.4g %s\n",
            SignalName(signal), mil.Setup().plantName.c_str(), *mil.Setpoint(), metrics->maxDeviation,
            metrics->recoveryTimeS * kMilliPerSecond, metrics->steadyStateError, SignalUnit(signal));
        return metrics;
    }
}
