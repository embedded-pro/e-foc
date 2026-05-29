#include "core/state_machine/ControlModeStateMachine.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/controllers/interfaces/PidController.hpp"

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
    {
        RegisterCliCommands();
        Activate(ControlModeFromRaw(configData.defaultControlMode));
    }

    void ControlModeStateMachine::Select(ControlMode mode, const infra::Function<void(SelectResult)>& onDone)
    {
        really_assert(pendingSelectCallback == nullptr);

        if (!IsStopped(ActiveStateMachine().CurrentState()))
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
            pendingSelectCallback(SelectResult::nvmFailed);
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
        return ControlMode::torque;
    }

    FocStateMachineBase& ControlModeStateMachine::ActiveStateMachine()
    {
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return *sm;
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return *sm;
        return std::get<application::TorqueStateMachine>(activeSm);
    }

    const FocStateMachineBase& ControlModeStateMachine::ActiveStateMachine() const
    {
        if (const auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return *sm;
        if (const auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return *sm;
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

    namespace
    {
        foc::IdAndIqTunings ToCurrentTunings(const services::FocPidGains& gains)
        {
            controllers::PidTunings<float> t{
                static_cast<float>(gains.kp),
                static_cast<float>(gains.ki),
                static_cast<float>(gains.kd)
            };
            return { t, t };
        }

        controllers::PidTunings<float> ToSpeedOrPositionTunings(const services::FocPidGains& gains)
        {
            return {
                static_cast<float>(gains.kp),
                static_cast<float>(gains.ki),
                static_cast<float>(gains.kd)
            };
        }
    }

    bool ControlModeStateMachine::TrySetCurrentPidGains(const services::FocPidGains& gains)
    {
        if (auto* sm = std::get_if<application::TorqueStateMachine>(&activeSm))
        {
            sm->GetController().SetCurrentTunings(hardware.vdc, ToCurrentTunings(gains));
            return true;
        }
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
        {
            sm->GetController().SetCurrentTunings(hardware.vdc, ToCurrentTunings(gains));
            return true;
        }
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
        {
            sm->GetController().SetCurrentTunings(hardware.vdc, ToCurrentTunings(gains));
            return true;
        }
        return false;
    }

    bool ControlModeStateMachine::TrySetSpeedPidGains(const services::FocPidGains& gains)
    {
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
        {
            sm->GetController().SetSpeedTunings(hardware.vdc, ToSpeedOrPositionTunings(gains));
            return true;
        }
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
        {
            sm->GetController().SetSpeedTunings(hardware.vdc, ToSpeedOrPositionTunings(gains));
            return true;
        }
        return false;
    }

    bool ControlModeStateMachine::TrySetPositionPidGains(const services::FocPidGains& gains)
    {
        auto* sm = std::get_if<application::PositionStateMachine>(&activeSm);
        if (sm == nullptr)
            return false;
        sm->GetController().SetPositionTunings(ToSpeedOrPositionTunings(gains));
        return true;
    }

    void ControlModeStateMachine::Activate(ControlMode mode)
    {
        if (!std::holds_alternative<std::monostate>(activeSm))
            ActiveStateMachine().CmdEmergencyStop();

        if (mode == ControlMode::speed)
            activeSm.emplace<application::SpeedStateMachine>(
                terminalAndTracer,
                hardware,
                nvm,
                calibServices,
                faultNotifier,
                TransitionPolicy::Auto,
                outerLoopArgs);
        else if (mode == ControlMode::position)
            activeSm.emplace<application::PositionStateMachine>(
                terminalAndTracer,
                hardware,
                nvm,
                calibServices,
                faultNotifier,
                TransitionPolicy::Auto,
                outerLoopArgs);
        else
            activeSm.emplace<application::TorqueStateMachine>(
                terminalAndTracer,
                hardware,
                nvm,
                calibServices,
                faultNotifier,
                TransitionPolicy::Auto);
    }

    void ControlModeStateMachine::RegisterCliCommands()
    {
        auto& terminal = terminalAndTracer.terminal;

        application::RegisterLifecycleCliCommands(terminal,
            [this]() -> FocStateMachineBase&
            {
                return ActiveStateMachine();
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
    }
}
