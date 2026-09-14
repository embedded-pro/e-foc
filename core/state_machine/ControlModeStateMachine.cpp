#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include "core/services/cli/TerminalHelper.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "infra/util/Tokenizer.hpp"
#include <optional>

namespace
{
    using CliStatus = services::TerminalWithStorage::Status;
    using CliStatusWithMessage = services::TerminalWithStorage::StatusWithMessage;

    constexpr auto wrongModeMessage = "rejected: command does not apply to the active control mode.";
    constexpr auto wrongStateMessage = "rejected: setpoints are only accepted in Ready or Enabled.";
    constexpr auto whileEnabledMessage = "rejected: bandwidth cannot be redesigned while enabled.";
    constexpr auto outOfRangeMessage = "invalid value: out of range.";

    CliStatusWithMessage ToCliResult(state_machine::TuningResult result)
    {
        switch (result)
        {
            case state_machine::TuningResult::ok:
                return CliStatusWithMessage{};
            case state_machine::TuningResult::outOfRange:
                return CliStatusWithMessage{ CliStatus::error, outOfRangeMessage };
            case state_machine::TuningResult::notWhileEnabled:
                return CliStatusWithMessage{ CliStatus::error, whileEnabledMessage };
            case state_machine::TuningResult::wrongMode:
                break;
        }

        return CliStatusWithMessage{ CliStatus::error, wrongModeMessage };
    }

    std::optional<CliStatusWithMessage> ParseSingleFloat(const infra::BoundedConstString& input, float& value, float minValue, float maxValue)
    {
        infra::Tokenizer tokenizer(input, ' ');

        if (tokenizer.Size() != 1)
            return CliStatusWithMessage{ CliStatus::error, "invalid number of arguments." };

        const auto parsed = services::ParseInput(tokenizer.Token(0));
        if (!parsed.has_value())
            return CliStatusWithMessage{ CliStatus::error, "invalid value. It should be a float." };

        if (!foc::IsWithinInclusive(*parsed, minValue, maxValue))
            return CliStatusWithMessage{ CliStatus::error, outOfRangeMessage };

        value = *parsed;
        return std::nullopt;
    }

    std::optional<foc::CurrentAlgorithm> ParseCurrentAlgorithm(const infra::BoundedConstString& name)
    {
        if (name == "pid")
            return foc::CurrentAlgorithm::pid;
        if (name == "decoupled")
            return foc::CurrentAlgorithm::decoupledPid;
        if (name == "deadbeat")
            return foc::CurrentAlgorithm::deadbeat;
        if (name == "sliding")
            return foc::CurrentAlgorithm::slidingMode;
        return std::nullopt;
    }

    std::optional<foc::SpeedAlgorithm> ParseSpeedAlgorithm(const infra::BoundedConstString& name)
    {
        if (name == "pid")
            return foc::SpeedAlgorithm::pid;
        if (name == "lqi")
            return foc::SpeedAlgorithm::lqi;
        if (name == "adrc")
            return foc::SpeedAlgorithm::adrc;
        if (name == "twodof")
            return foc::SpeedAlgorithm::twoDof;
        return std::nullopt;
    }

    std::optional<foc::PositionAlgorithm> ParsePositionAlgorithm(const infra::BoundedConstString& name)
    {
        if (name == "pid")
            return foc::PositionAlgorithm::pid;
        if (name == "cascadep")
            return foc::PositionAlgorithm::cascadeP;
        if (name == "lqr")
            return foc::PositionAlgorithm::lqr;
        if (name == "lqi")
            return foc::PositionAlgorithm::lqi;
        if (name == "twodof")
            return foc::PositionAlgorithm::twoDof;
        return std::nullopt;
    }
}

namespace state_machine
{
    ControlModeStateMachine::ControlModeStateMachine(
        const application::TerminalAndTracer& terminalAndTracer,
        const application::MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        const application::CalibrationServices& calibServices,
        FaultNotifier& faultNotifier,
        const services::ConfigData& configData,
        OuterLoopArgs outerLoopArgs)
        : terminalAndTracer(terminalAndTracer)
        , hardware(hardware)
        , nvm(nvm)
        , calibServices(calibServices)
        , faultNotifier(faultNotifier)
        , outerLoopArgs(outerLoopArgs)
        , configData(configData)
        , algorithmPersistence(nvm, this->configData, terminalAndTracer.tracer)
    {
        RegisterCliCommands();
        Activate(ControlModeFromRaw(configData.defaultControlMode));
    }

    void ControlModeStateMachine::Select(ControlMode mode, const infra::Function<void(SelectResult)>& onDone)
    {
        if (pendingSelectCallback != nullptr)
        {
            onDone(SelectResult::busy);
            return;
        }

        if (!IsStopped(ActiveStateMachine().CurrentState()) || ActiveStateMachine().HasPendingAsyncWork())
        {
            onDone(SelectResult::busy);
            return;
        }

        previousDefaultControlMode = configData.defaultControlMode;
        configData.defaultControlMode = static_cast<uint8_t>(mode);
        pendingSelectMode = mode;
        pendingSelectCallback = onDone;
        nvm.SaveConfig(configData, [this](services::NvmStatus status)
            {
                OnSaveConfigDone(status);
            });
    }

    void ControlModeStateMachine::OnSaveConfigDone(services::NvmStatus status)
    {
        if (status != services::NvmStatus::Ok)
        {
            configData.defaultControlMode = previousDefaultControlMode;
            pendingSelectCallback(status == services::NvmStatus::Busy ? SelectResult::busy : SelectResult::nvmFailed);
        }
        else
        {
            Activate(pendingSelectMode);
            pendingSelectCallback(SelectResult::ok);
        }
    }

    ControlMode ControlModeStateMachine::Active() const
    {
        if (std::holds_alternative<application::SpeedStateMachine>(activeSm))
            return ControlMode::speed;
        if (std::holds_alternative<application::PositionStateMachine>(activeSm))
            return ControlMode::position;
        really_assert(std::holds_alternative<application::TorqueStateMachine>(activeSm));
        return ControlMode::torque;
    }

    FocStateMachineBase& ControlModeStateMachine::ActiveStateMachine()
    {
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return *sm;
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return *sm;
        really_assert(std::holds_alternative<application::TorqueStateMachine>(activeSm));
        return std::get<application::TorqueStateMachine>(activeSm);
    }

    const FocStateMachineBase& ControlModeStateMachine::ActiveStateMachine() const
    {
        if (const auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return *sm;
        if (const auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return *sm;
        really_assert(std::holds_alternative<application::TorqueStateMachine>(activeSm));
        return std::get<application::TorqueStateMachine>(activeSm);
    }

    bool ControlModeStateMachine::TrySetTorque(foc::IdAndIqPoint setpoint)
    {
        auto* sm = std::get_if<application::TorqueStateMachine>(&activeSm);
        if (sm == nullptr)
            return false;
        sm->GetController().SetPoint(setpoint);
        return true;
    }

    bool ControlModeStateMachine::TrySetSpeed(foc::RadiansPerSecond setpoint)
    {
        auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm);
        if (sm == nullptr)
            return false;
        sm->GetController().SetPoint(setpoint);
        return true;
    }

    bool ControlModeStateMachine::TrySetPosition(foc::Radians setpoint)
    {
        auto* sm = std::get_if<application::PositionStateMachine>(&activeSm);
        if (sm == nullptr)
            return false;
        sm->GetController().SetPoint(setpoint);
        return true;
    }

    TuningResult ControlModeStateMachine::CheckRedesignPreconditions() const
    {
        if (std::holds_alternative<std::monostate>(activeSm))
            return TuningResult::wrongMode;

        if (std::holds_alternative<Enabled>(ActiveStateMachine().CurrentState()))
            return TuningResult::notWhileEnabled;

        return TuningResult::ok;
    }

    TuningResult ControlModeStateMachine::TrySetCurrentBandwidth(float bandwidth)
    {
        if (!foc::IsAcceptableCurrentBandwidth(bandwidth))
            return TuningResult::outOfRange;

        if (const auto rejection = CheckRedesignPreconditions(); rejection != TuningResult::ok)
            return rejection;

        auto tunings = foc::CurrentLoopTunings{};
        tunings.bandwidth = bandwidth;

        if (auto* sm = std::get_if<application::TorqueStateMachine>(&activeSm))
            sm->GetController().SetCurrentTunings(tunings);
        else if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            sm->GetController().SetCurrentTunings(tunings);
        else if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            sm->GetController().SetCurrentTunings(tunings);
        else
            return TuningResult::wrongMode;

        return TuningResult::ok;
    }

    TuningResult ControlModeStateMachine::TrySetSpeedBandwidth(float bandwidth)
    {
        if (!foc::IsAcceptableSpeedBandwidth(bandwidth))
            return TuningResult::outOfRange;

        if (const auto rejection = CheckRedesignPreconditions(); rejection != TuningResult::ok)
            return rejection;

        auto tunings = foc::SpeedLoopTunings{};
        tunings.bandwidth = bandwidth;

        if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            sm->GetController().SetSpeedTunings(tunings);
        else if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            sm->GetController().SetSpeedTunings(tunings);
        else
            return TuningResult::wrongMode;

        return TuningResult::ok;
    }

    TuningResult ControlModeStateMachine::TrySetPositionBandwidth(float bandwidth)
    {
        if (!foc::IsAcceptablePositionBandwidth(bandwidth))
            return TuningResult::outOfRange;

        if (const auto rejection = CheckRedesignPreconditions(); rejection != TuningResult::ok)
            return rejection;

        auto* sm = std::get_if<application::PositionStateMachine>(&activeSm);
        if (sm == nullptr)
            return TuningResult::wrongMode;

        auto tunings = foc::PositionLoopTunings{};
        tunings.bandwidth = bandwidth;

        return sm->GetController().SetPositionTunings(tunings) == foc::SelectResult::ok ? TuningResult::ok : TuningResult::outOfRange;
    }

    void ControlModeStateMachine::Activate(ControlMode mode)
    {
        if (!std::holds_alternative<std::monostate>(activeSm))
            ActiveStateMachine().CmdEmergencyStop();

        if (mode == ControlMode::speed)
            AttachAlgorithmRestore(activeSm.emplace<application::SpeedStateMachine>(
                terminalAndTracer,
                hardware,
                nvm,
                calibServices,
                faultNotifier,
                TransitionPolicy::Auto,
                outerLoopArgs));
        else if (mode == ControlMode::position)
            AttachAlgorithmRestore(activeSm.emplace<application::PositionStateMachine>(
                terminalAndTracer,
                hardware,
                nvm,
                calibServices,
                faultNotifier,
                TransitionPolicy::Auto,
                outerLoopArgs));
        else
            AttachAlgorithmRestore(activeSm.emplace<application::TorqueStateMachine>(
                terminalAndTracer,
                hardware,
                nvm,
                calibServices,
                faultNotifier,
                TransitionPolicy::Auto));
    }

    void ControlModeStateMachine::AttachAlgorithmRestore(application::FocStateMachineCommon& stateMachine)
    {
        stateMachine.RegisterReadyHandler([this]()
            {
                algorithmPersistence.ApplyPersistedAlgorithms(CurrentSelectable(), SpeedSelectable(), PositionSelectable());
            });

        if (std::holds_alternative<Ready>(stateMachine.CurrentState()))
            algorithmPersistence.ApplyPersistedAlgorithms(CurrentSelectable(), SpeedSelectable(), PositionSelectable());
    }

    foc::CurrentLoopSelectable* ControlModeStateMachine::CurrentSelectable()
    {
        if (auto* sm = std::get_if<application::TorqueStateMachine>(&activeSm))
            return &sm->GetController();
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return &sm->GetController();
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return &sm->GetController();
        return nullptr;
    }

    foc::SpeedLoopSelectable* ControlModeStateMachine::SpeedSelectable()
    {
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return &sm->GetController();
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return &sm->GetController();
        return nullptr;
    }

    foc::PositionLoopSelectable* ControlModeStateMachine::PositionSelectable()
    {
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return &sm->GetController();
        return nullptr;
    }

    const foc::CurrentLoopSelectable* ControlModeStateMachine::CurrentSelectable() const
    {
        if (const auto* sm = std::get_if<application::TorqueStateMachine>(&activeSm))
            return &sm->GetController();
        if (const auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return &sm->GetController();
        if (const auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return &sm->GetController();
        return nullptr;
    }

    const foc::SpeedLoopSelectable* ControlModeStateMachine::SpeedSelectable() const
    {
        if (const auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return &sm->GetController();
        if (const auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return &sm->GetController();
        return nullptr;
    }

    const foc::PositionLoopSelectable* ControlModeStateMachine::PositionSelectable() const
    {
        if (const auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return &sm->GetController();
        return nullptr;
    }

    foc::SelectResult ControlModeStateMachine::SelectCurrentAlgorithm(foc::CurrentAlgorithm algorithm)
    {
        return algorithmPersistence.SelectCurrentAlgorithm(algorithm, CurrentSelectable());
    }

    foc::SelectResult ControlModeStateMachine::SelectSpeedAlgorithm(foc::SpeedAlgorithm algorithm)
    {
        return algorithmPersistence.SelectSpeedAlgorithm(algorithm, SpeedSelectable());
    }

    foc::SelectResult ControlModeStateMachine::SelectPositionAlgorithm(foc::PositionAlgorithm algorithm)
    {
        return algorithmPersistence.SelectPositionAlgorithm(algorithm, PositionSelectable());
    }

    foc::CurrentAlgorithm ControlModeStateMachine::ActiveCurrentAlgorithm() const
    {
        return algorithmPersistence.ActiveCurrentAlgorithm(CurrentSelectable());
    }

    foc::SpeedAlgorithm ControlModeStateMachine::ActiveSpeedAlgorithm() const
    {
        return algorithmPersistence.ActiveSpeedAlgorithm(SpeedSelectable());
    }

    foc::PositionAlgorithm ControlModeStateMachine::ActivePositionAlgorithm() const
    {
        return algorithmPersistence.ActivePositionAlgorithm(PositionSelectable());
    }

    void ControlModeStateMachine::RegisterCliCommands()
    {
        auto& terminal = terminalAndTracer.terminal;

        application::RegisterLifecycleCliCommands(terminal,
            [this]() -> application::FocStateMachineCommon&
            {
                return ActiveCommon();
            });

        terminal.AddCommand({ { "apply_estimates", "ae", "Apply online estimates to PID gains" },
            [this](const infra::BoundedConstString&)
            {
                ActiveStateMachine().ApplyOnlineEstimates();
            } });

        terminal.AddCommand({ { "active_mode", "am", "Print the active control mode" },
            [this](const infra::BoundedConstString&)
            {
                const auto activeMode = Active();
                if (activeMode == ControlMode::speed)
                    terminalAndTracer.tracer.Trace() << "Active mode: speed";
                else if (activeMode == ControlMode::position)
                    terminalAndTracer.tracer.Trace() << "Active mode: position";
                else
                    terminalAndTracer.tracer.Trace() << "Active mode: torque";
            } });

        terminal.AddCommand({ { "select_current_algorithm", "sca", "Select current loop algorithm [pid|decoupled|deadbeat|sliding]. Ex: sca deadbeat" },
            [this](const infra::BoundedConstString& param)
            {
                const auto algorithm = ParseCurrentAlgorithm(param);
                if (!algorithm)
                    terminalAndTracer.tracer.Trace() << "Unknown algorithm. Expected pid, decoupled, deadbeat or sliding";
                else
                    TraceSelectResult(SelectCurrentAlgorithm(*algorithm));
            } });

        terminal.AddCommand({ { "select_speed_algorithm", "ssa", "Select speed loop algorithm [pid|lqi|adrc|twodof]. Ex: ssa lqi" },
            [this](const infra::BoundedConstString& param)
            {
                const auto algorithm = ParseSpeedAlgorithm(param);
                if (!algorithm)
                    terminalAndTracer.tracer.Trace() << "Unknown algorithm. Expected pid, lqi, adrc or twodof";
                else
                    TraceSelectResult(SelectSpeedAlgorithm(*algorithm));
            } });

        terminal.AddCommand({ { "select_position_algorithm", "spa", "Select position loop algorithm [pid|cascadep|lqr|lqi|twodof]. Ex: spa lqi" },
            [this](const infra::BoundedConstString& param)
            {
                const auto algorithm = ParsePositionAlgorithm(param);
                if (!algorithm)
                    terminalAndTracer.tracer.Trace() << "Unknown algorithm. Expected pid, cascadep, lqr, lqi or twodof";
                else
                    TraceSelectResult(SelectPositionAlgorithm(*algorithm));
            } });

        terminal.AddCommand({ { "active_algorithms", "aa", "Print the active loop algorithms" },
            [this](const infra::BoundedConstString&)
            {
                terminalAndTracer.tracer.Trace() << "Current loop: " << AlgorithmPersistence::CurrentAlgorithmName(ActiveCurrentAlgorithm());
                terminalAndTracer.tracer.Trace() << "Speed loop: " << AlgorithmPersistence::SpeedAlgorithmName(ActiveSpeedAlgorithm());
                terminalAndTracer.tracer.Trace() << "Position loop: " << AlgorithmPersistence::PositionAlgorithmName(ActivePositionAlgorithm());
            } });

        terminal.AddCommand({ { "estimate_status", "es", "Print current online estimates" },
            [this](const infra::BoundedConstString&)
            {
                if (auto* outerLoop = ActiveOuterLoop())
                    outerLoop->TraceOnlineEstimates();
                else
                    terminalAndTracer.tracer.Trace() << "Rejected: online estimates are not available in torque mode";
            } });

        terminal.AddCommand({ { "set_flux_linkage", "sfl", "Set and persist rotor flux linkage in Wb. set_flux_linkage <psi>. Ex: sfl 0.007" },
            [this, &terminal](const infra::BoundedConstString& params)
            {
                terminal.ProcessResult(SetFluxLinkageFromCli(params));
            } });

        RegisterSetpointCliCommands(terminal);
        RegisterBandwidthCliCommands(terminal);
    }

    void ControlModeStateMachine::RegisterSetpointCliCommands(services::TerminalWithStorage& terminal)
    {
        terminal.AddCommand({ { "set_torque", "st", "Set q-axis current in A, torque mode only. set_torque <iq>. Ex: st 2.5" },
            [this, &terminal](const infra::BoundedConstString& params)
            {
                terminal.ProcessResult(SetTorqueSetpoint(params));
            } });

        terminal.AddCommand({ { "set_speed", "ss", "Set speed in rad/s, speed mode only. set_speed <speed>. Ex: ss 20.0" },
            [this, &terminal](const infra::BoundedConstString& params)
            {
                terminal.ProcessResult(SetSpeedSetpoint(params));
            } });

        terminal.AddCommand({ { "set_position", "sp", "Set mechanical position in rad, position mode only. set_position <position>. Ex: sp 3.14" },
            [this, &terminal](const infra::BoundedConstString& params)
            {
                terminal.ProcessResult(SetPositionSetpoint(params));
            } });
    }

    void ControlModeStateMachine::RegisterBandwidthCliCommands(services::TerminalWithStorage& terminal)
    {
        terminal.AddCommand({ { "set_current_bandwidth", "scbw", "Set current loop bandwidth in rad/s. set_current_bandwidth <bandwidth>. Ex: scbw 6283.2" },
            [this, &terminal](const infra::BoundedConstString& params)
            {
                terminal.ProcessResult(SetCurrentBandwidth(params));
            } });

        terminal.AddCommand({ { "set_speed_bandwidth", "ssbw", "Set speed loop bandwidth in rad/s. set_speed_bandwidth <bandwidth>. Ex: ssbw 188.5" },
            [this, &terminal](const infra::BoundedConstString& params)
            {
                terminal.ProcessResult(SetSpeedBandwidth(params));
            } });

        terminal.AddCommand({ { "set_position_bandwidth", "spbw", "Set position loop bandwidth in rad/s. set_position_bandwidth <bandwidth>. Ex: spbw 18.8" },
            [this, &terminal](const infra::BoundedConstString& params)
            {
                terminal.ProcessResult(SetPositionBandwidth(params));
            } });
    }

    std::optional<ControlModeStateMachine::CliResult> ControlModeStateMachine::RejectSetpoint(ControlMode requiredMode) const
    {
        if (Active() != requiredMode)
            return CliResult{ CliStatus::error, wrongModeMessage };

        const auto& state = ActiveStateMachine().CurrentState();
        if (!std::holds_alternative<Ready>(state) && !std::holds_alternative<Enabled>(state))
            return CliResult{ CliStatus::error, wrongStateMessage };

        return std::nullopt;
    }

    ControlModeStateMachine::CliResult ControlModeStateMachine::SetTorqueSetpoint(const infra::BoundedConstString& input)
    {
        float value{ 0.0f };
        if (auto error = ParseSingleFloat(input, value, -foc::CommandLimits::maxTorqueSetpoint, foc::CommandLimits::maxTorqueSetpoint); error.has_value())
            return *error;

        if (auto rejection = RejectSetpoint(ControlMode::torque); rejection.has_value())
            return *rejection;

        TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ value } });
        return CliResult{};
    }

    ControlModeStateMachine::CliResult ControlModeStateMachine::SetSpeedSetpoint(const infra::BoundedConstString& input)
    {
        float value{ 0.0f };
        if (auto error = ParseSingleFloat(input, value, -foc::CommandLimits::maxSpeedSetpoint, foc::CommandLimits::maxSpeedSetpoint); error.has_value())
            return *error;

        if (auto rejection = RejectSetpoint(ControlMode::speed); rejection.has_value())
            return *rejection;

        TrySetSpeed(foc::RadiansPerSecond{ value });
        return CliResult{};
    }

    ControlModeStateMachine::CliResult ControlModeStateMachine::SetPositionSetpoint(const infra::BoundedConstString& input)
    {
        float value{ 0.0f };
        if (auto error = ParseSingleFloat(input, value, -foc::CommandLimits::maxPositionSetpoint, foc::CommandLimits::maxPositionSetpoint); error.has_value())
            return *error;

        if (auto rejection = RejectSetpoint(ControlMode::position); rejection.has_value())
            return *rejection;

        TrySetPosition(foc::Radians{ value });
        return CliResult{};
    }

    ControlModeStateMachine::CliResult ControlModeStateMachine::SetCurrentBandwidth(const infra::BoundedConstString& input)
    {
        float value{ 0.0f };
        if (auto error = ParseSingleFloat(input, value, foc::CommandLimits::minBandwidth, foc::CommandLimits::maxCurrentBandwidth); error.has_value())
            return *error;

        return ToCliResult(TrySetCurrentBandwidth(value));
    }

    ControlModeStateMachine::CliResult ControlModeStateMachine::SetSpeedBandwidth(const infra::BoundedConstString& input)
    {
        float value{ 0.0f };
        if (auto error = ParseSingleFloat(input, value, foc::CommandLimits::minBandwidth, foc::CommandLimits::maxSpeedBandwidth); error.has_value())
            return *error;

        return ToCliResult(TrySetSpeedBandwidth(value));
    }

    ControlModeStateMachine::CliResult ControlModeStateMachine::SetPositionBandwidth(const infra::BoundedConstString& input)
    {
        float value{ 0.0f };
        if (auto error = ParseSingleFloat(input, value, foc::CommandLimits::minBandwidth, foc::CommandLimits::maxPositionBandwidth); error.has_value())
            return *error;

        if (Active() != ControlMode::position)
            return CliResult{ CliStatus::error, wrongModeMessage };

        // Retuning redesigns the position law, which can be refused; never report that as applied
        if (const auto result = TrySetPositionBandwidth(value); result == TuningResult::outOfRange)
            return CliResult{ CliStatus::error, "rejected: no controller for this bandwidth." };
        else
            return ToCliResult(result);
    }

    ControlModeStateMachine::CliResult ControlModeStateMachine::SetFluxLinkageFromCli(const infra::BoundedConstString& input)
    {
        float value{ 0.0f };
        if (auto error = ParseSingleFloat(input, value, 0.0f, foc::CommandLimits::maxFluxLinkage); error.has_value())
            return *error;

        // The persist completes asynchronously, so the state machine traces the outcome.
        SetFluxLinkage(foc::Weber{ value }, [](CommandResult) {});
        return CliResult{};
    }

    application::OuterLoopStateMachine* ControlModeStateMachine::ActiveOuterLoop()
    {
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return sm;
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return sm;
        return nullptr;
    }

    application::FocStateMachineCommon& ControlModeStateMachine::ActiveCommon()
    {
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return *sm;
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return *sm;
        return std::get<application::TorqueStateMachine>(activeSm);
    }

    const application::FocStateMachineCommon& ControlModeStateMachine::ActiveCommon() const
    {
        if (const auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return *sm;
        if (const auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return *sm;
        return std::get<application::TorqueStateMachine>(activeSm);
    }

    void ControlModeStateMachine::SetFluxLinkage(foc::Weber fluxLinkage, const infra::Function<void(CommandResult)>& onDone)
    {
        ActiveCommon().CmdSetFluxLinkage(fluxLinkage, onDone);
    }

    CommandResult ControlModeStateMachine::CmdReserveExternalCalibration()
    {
        return ActiveCommon().CmdReserveExternalCalibration();
    }

    void ControlModeStateMachine::CmdCompleteExternalCalibration(const services::CalibrationData& data,
        const infra::Function<void(CommandResult)>& onDone)
    {
        ActiveCommon().CmdCompleteExternalCalibration(data, onDone);
    }

    void ControlModeStateMachine::CmdReAlign(const infra::Function<void(CommandResult)>& onDone)
    {
        ActiveCommon().CmdReAlign(onDone);
    }

    std::optional<services::CalibrationData> ControlModeStateMachine::ActiveCalibrationData() const
    {
        const auto& state = ActiveStateMachine().CurrentState();
        if (const auto* ready = std::get_if<state_machine::Ready>(&state))
            return ready->loadedData;
        return std::nullopt;
    }

    foc::Weber ControlModeStateMachine::ActiveFluxLinkage() const
    {
        return ActiveCommon().ActiveFluxLinkage();
    }

    void ControlModeStateMachine::TraceSelectResult(foc::SelectResult result) const
    {
        switch (result)
        {
            case foc::SelectResult::ok:
                terminalAndTracer.tracer.Trace() << "Algorithm selected";
                break;
            case foc::SelectResult::busy:
                terminalAndTracer.tracer.Trace() << "Rejected: motor is enabled";
                break;
            case foc::SelectResult::invalidParameters:
                terminalAndTracer.tracer.Trace() << "Rejected: motor model not identified";
                break;
            case foc::SelectResult::invalidAlgorithm:
                terminalAndTracer.tracer.Trace() << "Rejected: algorithm not available in this control mode";
                break;
        }
    }
}
