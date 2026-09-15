#include "core/state_machine/ControlModeCliCommands.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include "core/services/cli/TerminalHelper.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/FocStateMachineCommon.hpp"
#include "infra/util/Tokenizer.hpp"
#include <optional>

namespace
{
    using CliStatus = services::TerminalWithStorage::Status;
    using CliResult = services::TerminalWithStorage::StatusWithMessage;

    constexpr auto wrongModeMessage = "rejected: command does not apply to the active control mode.";
    constexpr auto wrongStateMessage = "rejected: setpoints are only accepted in Ready or Enabled.";
    constexpr auto whileEnabledMessage = "rejected: bandwidth cannot be redesigned while enabled.";
    constexpr auto outOfRangeMessage = "invalid value: out of range.";

    CliResult ToCliResult(state_machine::TuningResult result)
    {
        switch (result)
        {
            case state_machine::TuningResult::ok:
                return CliResult{};
            case state_machine::TuningResult::outOfRange:
                return CliResult{ CliStatus::error, outOfRangeMessage };
            case state_machine::TuningResult::notWhileEnabled:
                return CliResult{ CliStatus::error, whileEnabledMessage };
            case state_machine::TuningResult::wrongMode:
                break;
        }

        return CliResult{ CliStatus::error, wrongModeMessage };
    }

    std::optional<CliResult> ParseSingleFloat(const infra::BoundedConstString& input, float& value, float minValue, float maxValue)
    {
        infra::Tokenizer tokenizer(input, ' ');

        if (tokenizer.Size() != 1)
            return CliResult{ CliStatus::error, "invalid number of arguments." };

        const auto parsed = services::ParseInput(tokenizer.Token(0));
        if (!parsed.has_value())
            return CliResult{ CliStatus::error, "invalid value. It should be a float." };

        if (!foc::IsWithinInclusive(*parsed, minValue, maxValue))
            return CliResult{ CliStatus::error, outOfRangeMessage };

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

    void TraceSelectResult(foc::SelectResult result, services::Tracer& tracer)
    {
        switch (result)
        {
            case foc::SelectResult::ok:
                tracer.Trace() << "Algorithm selected";
                break;
            case foc::SelectResult::busy:
                tracer.Trace() << "Rejected: motor is enabled";
                break;
            case foc::SelectResult::invalidParameters:
                tracer.Trace() << "Rejected: motor model not identified";
                break;
            case foc::SelectResult::invalidAlgorithm:
                tracer.Trace() << "Rejected: algorithm not available in this control mode";
                break;
        }
    }

    std::optional<CliResult> RejectSetpoint(
        state_machine::ControlMode requiredMode,
        state_machine::ControlModeStateMachine& sm)
    {
        using namespace state_machine;

        if (sm.Active() != requiredMode)
            return CliResult{ CliStatus::error, wrongModeMessage };

        const auto& state = sm.ActiveStateMachine().CurrentState();
        if (!std::holds_alternative<Ready>(state) && !std::holds_alternative<Enabled>(state))
            return CliResult{ CliStatus::error, wrongStateMessage };

        return std::nullopt;
    }

    void RegisterAlgorithmCliCommands(
        services::TerminalWithStorage& terminal,
        state_machine::ControlModeStateMachine& sm,
        services::Tracer& tracer)
    {
        terminal.AddCommand({ { "select_current_algorithm", "sca", "Select current loop algorithm [pid|decoupled|deadbeat|sliding]. Ex: sca deadbeat" },
            [&sm, &tracer](const infra::BoundedConstString& param)
            {
                const auto algorithm = ParseCurrentAlgorithm(param);
                if (!algorithm)
                    tracer.Trace() << "Unknown algorithm. Expected pid, decoupled, deadbeat or sliding";
                else
                    TraceSelectResult(sm.SelectCurrentAlgorithm(*algorithm), tracer);
            } });

        terminal.AddCommand({ { "select_speed_algorithm", "ssa", "Select speed loop algorithm [pid|lqi|adrc|twodof]. Ex: ssa lqi" },
            [&sm, &tracer](const infra::BoundedConstString& param)
            {
                const auto algorithm = ParseSpeedAlgorithm(param);
                if (!algorithm)
                    tracer.Trace() << "Unknown algorithm. Expected pid, lqi, adrc or twodof";
                else
                    TraceSelectResult(sm.SelectSpeedAlgorithm(*algorithm), tracer);
            } });

        terminal.AddCommand({ { "select_position_algorithm", "spa", "Select position loop algorithm [pid|cascadep|lqr|lqi|twodof]. Ex: spa lqi" },
            [&sm, &tracer](const infra::BoundedConstString& param)
            {
                const auto algorithm = ParsePositionAlgorithm(param);
                if (!algorithm)
                    tracer.Trace() << "Unknown algorithm. Expected pid, cascadep, lqr, lqi or twodof";
                else
                    TraceSelectResult(sm.SelectPositionAlgorithm(*algorithm), tracer);
            } });

        terminal.AddCommand({ { "active_algorithms", "aa", "Print the active loop algorithms" },
            [&sm, &tracer](const infra::BoundedConstString&)
            {
                tracer.Trace() << "Current loop: " << state_machine::AlgorithmPersistence::CurrentAlgorithmName(sm.ActiveCurrentAlgorithm());
                tracer.Trace() << "Speed loop: " << state_machine::AlgorithmPersistence::SpeedAlgorithmName(sm.ActiveSpeedAlgorithm());
                tracer.Trace() << "Position loop: " << state_machine::AlgorithmPersistence::PositionAlgorithmName(sm.ActivePositionAlgorithm());
            } });
    }

    void RegisterSetpointCliCommands(
        services::TerminalWithStorage& terminal,
        state_machine::ControlModeStateMachine& sm)
    {
        terminal.AddCommand({ { "set_flux_linkage", "sfl", "Set and persist rotor flux linkage in Wb. set_flux_linkage <psi>. Ex: sfl 0.007" },
            [&sm, &terminal](const infra::BoundedConstString& params)
            {
                float value{ 0.0f };
                if (auto error = ParseSingleFloat(params, value, 0.0f, foc::CommandLimits::maxFluxLinkage); error.has_value())
                {
                    terminal.ProcessResult(*error);
                    return;
                }
                sm.SetFluxLinkage(foc::Weber{ value }, [](state_machine::CommandResult) {});
                terminal.ProcessResult(CliResult{});
            } });

        terminal.AddCommand({ { "set_torque", "st", "Set q-axis current in A, torque mode only. set_torque <iq>. Ex: st 2.5" },
            [&sm, &terminal](const infra::BoundedConstString& params)
            {
                float value{ 0.0f };
                if (auto error = ParseSingleFloat(params, value, -foc::CommandLimits::maxTorqueSetpoint, foc::CommandLimits::maxTorqueSetpoint); error.has_value())
                {
                    terminal.ProcessResult(*error);
                    return;
                }
                if (auto rejection = RejectSetpoint(state_machine::ControlMode::torque, sm); rejection.has_value())
                {
                    terminal.ProcessResult(*rejection);
                    return;
                }
                sm.TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ value } });
                terminal.ProcessResult(CliResult{});
            } });

        terminal.AddCommand({ { "set_speed", "ss", "Set speed in rad/s, speed mode only. set_speed <speed>. Ex: ss 20.0" },
            [&sm, &terminal](const infra::BoundedConstString& params)
            {
                float value{ 0.0f };
                if (auto error = ParseSingleFloat(params, value, -foc::CommandLimits::maxSpeedSetpoint, foc::CommandLimits::maxSpeedSetpoint); error.has_value())
                {
                    terminal.ProcessResult(*error);
                    return;
                }
                if (auto rejection = RejectSetpoint(state_machine::ControlMode::speed, sm); rejection.has_value())
                {
                    terminal.ProcessResult(*rejection);
                    return;
                }
                sm.TrySetSpeed(foc::RadiansPerSecond{ value });
                terminal.ProcessResult(CliResult{});
            } });

        terminal.AddCommand({ { "set_position", "sp", "Set mechanical position in rad, position mode only. set_position <position>. Ex: sp 3.14" },
            [&sm, &terminal](const infra::BoundedConstString& params)
            {
                float value{ 0.0f };
                if (auto error = ParseSingleFloat(params, value, -foc::CommandLimits::maxPositionSetpoint, foc::CommandLimits::maxPositionSetpoint); error.has_value())
                {
                    terminal.ProcessResult(*error);
                    return;
                }
                if (auto rejection = RejectSetpoint(state_machine::ControlMode::position, sm); rejection.has_value())
                {
                    terminal.ProcessResult(*rejection);
                    return;
                }
                sm.TrySetPosition(foc::Radians{ value });
                terminal.ProcessResult(CliResult{});
            } });
    }

    void RegisterBandwidthCliCommands(
        services::TerminalWithStorage& terminal,
        state_machine::ControlModeStateMachine& sm)
    {
        terminal.AddCommand({ { "set_current_bandwidth", "scbw", "Set current loop bandwidth in rad/s. set_current_bandwidth <bandwidth>. Ex: scbw 6283.2" },
            [&sm, &terminal](const infra::BoundedConstString& params)
            {
                float value{ 0.0f };
                if (auto error = ParseSingleFloat(params, value, foc::CommandLimits::minBandwidth, foc::CommandLimits::maxCurrentBandwidth); error.has_value())
                {
                    terminal.ProcessResult(*error);
                    return;
                }
                terminal.ProcessResult(ToCliResult(sm.TrySetCurrentBandwidth(value)));
            } });

        terminal.AddCommand({ { "set_speed_bandwidth", "ssbw", "Set speed loop bandwidth in rad/s. set_speed_bandwidth <bandwidth>. Ex: ssbw 188.5" },
            [&sm, &terminal](const infra::BoundedConstString& params)
            {
                float value{ 0.0f };
                if (auto error = ParseSingleFloat(params, value, foc::CommandLimits::minBandwidth, foc::CommandLimits::maxSpeedBandwidth); error.has_value())
                {
                    terminal.ProcessResult(*error);
                    return;
                }
                terminal.ProcessResult(ToCliResult(sm.TrySetSpeedBandwidth(value)));
            } });

        terminal.AddCommand({ { "set_position_bandwidth", "spbw", "Set position loop bandwidth in rad/s. set_position_bandwidth <bandwidth>. Ex: spbw 18.8" },
            [&sm, &terminal](const infra::BoundedConstString& params)
            {
                float value{ 0.0f };
                if (auto error = ParseSingleFloat(params, value, foc::CommandLimits::minBandwidth, foc::CommandLimits::maxPositionBandwidth); error.has_value())
                {
                    terminal.ProcessResult(*error);
                    return;
                }
                if (sm.Active() != state_machine::ControlMode::position)
                {
                    terminal.ProcessResult(CliResult{ CliStatus::error, wrongModeMessage });
                    return;
                }
                if (const auto result = sm.TrySetPositionBandwidth(value); result == state_machine::TuningResult::outOfRange)
                    terminal.ProcessResult(CliResult{ CliStatus::error, "rejected: no controller for this bandwidth." });
                else
                    terminal.ProcessResult(ToCliResult(result));
            } });
    }
}

namespace state_machine
{
    void RegisterControlModeCliCommands(services::TerminalWithStorage& terminal, ControlModeStateMachine& sm, services::Tracer& tracer)
    {
        application::RegisterLifecycleCliCommands(terminal,
            [&sm]() -> application::FocStateMachineCommon&
            {
                return sm.ActiveCommon();
            });

        terminal.AddCommand({ { "apply_estimates", "ae", "Apply online estimates to PID gains" },
            [&sm](const infra::BoundedConstString&)
            {
                sm.ActiveStateMachine().ApplyOnlineEstimates();
            } });

        terminal.AddCommand({ { "active_mode", "am", "Print the active control mode" },
            [&sm, &tracer](const infra::BoundedConstString&)
            {
                const auto activeMode = sm.Active();
                if (activeMode == ControlMode::speed)
                    tracer.Trace() << "Active mode: speed";
                else if (activeMode == ControlMode::position)
                    tracer.Trace() << "Active mode: position";
                else
                    tracer.Trace() << "Active mode: torque";
            } });

        terminal.AddCommand({ { "estimate_status", "es", "Print current online estimates" },
            [&sm, &tracer](const infra::BoundedConstString&)
            {
                if (auto* outerLoop = sm.ActiveOuterLoop())
                    outerLoop->TraceOnlineEstimates();
                else
                    tracer.Trace() << "Rejected: online estimates are not available in torque mode";
            } });

        RegisterAlgorithmCliCommands(terminal, sm, tracer);
        RegisterSetpointCliCommands(terminal, sm);
        RegisterBandwidthCliCommands(terminal, sm);
    }
}
