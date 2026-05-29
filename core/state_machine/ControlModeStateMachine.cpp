#include "core/state_machine/ControlModeStateMachine.hpp"

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
        if (pendingSelectCallback != nullptr)
        {
            onDone(SelectResult::busy);
            return;
        }

        if (!IsMotorStopped())
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
            auto callback = std::move(pendingSelectCallback);
            pendingSelectCallback = nullptr;
            callback(SelectResult::nvmFailed);
        }
        else
        {
            Activate(pendingSelectMode);
            auto callback = std::move(pendingSelectCallback);
            pendingSelectCallback = nullptr;
            callback(SelectResult::ok);
        }
    }

    bool ControlModeStateMachine::SelectInProgress() const
    {
        return pendingSelectCallback != nullptr;
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

    bool ControlModeStateMachine::IsMotorStopped() const
    {
        return IsStopped(ActiveStateMachine().CurrentState());
    }

    void ControlModeStateMachine::Activate(ControlMode mode)
    {
        if (!std::holds_alternative<std::monostate>(activeSm))
            ActiveStateMachine().CmdEmergencyStop();

        switch (mode)
        {
            case ControlMode::speed:
                activeSm.emplace<application::SpeedStateMachine>(
                    terminalAndTracer,
                    hardware,
                    nvm,
                    calibServices,
                    faultNotifier,
                    TransitionPolicy::Auto,
                    outerLoopArgs.maxCurrent,
                    outerLoopArgs.baseFrequency,
                    outerLoopArgs.lowPriorityInterrupt);
                break;
            case ControlMode::position:
                activeSm.emplace<application::PositionStateMachine>(
                    terminalAndTracer,
                    hardware,
                    nvm,
                    calibServices,
                    faultNotifier,
                    TransitionPolicy::Auto,
                    outerLoopArgs.maxCurrent,
                    outerLoopArgs.baseFrequency,
                    outerLoopArgs.lowPriorityInterrupt);
                break;
            case ControlMode::torque:
            default:
                activeSm.emplace<application::TorqueStateMachine>(
                    terminalAndTracer,
                    hardware,
                    nvm,
                    calibServices,
                    faultNotifier,
                    TransitionPolicy::Auto);
                break;
        }
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
                switch (Active())
                {
                    case ControlMode::speed:
                        terminalAndTracer.tracer.Trace() << "Active mode: speed";
                        break;
                    case ControlMode::position:
                        terminalAndTracer.tracer.Trace() << "Active mode: position";
                        break;
                    case ControlMode::torque:
                    default:
                        terminalAndTracer.tracer.Trace() << "Active mode: torque";
                        break;
                }
            } });
    }
}
