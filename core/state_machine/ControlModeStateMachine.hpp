#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/services/cli/TerminalNoOp.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/state_machine/FocStateMachineImpl.hpp"
#include "core/state_machine/TransitionPolicies.hpp"
#include "infra/util/Function.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include <variant>

namespace state_machine
{
    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    class ControlModeStateMachine
    {
        using NoOp = services::TerminalFocNoOpInteractor;

    public:
        using TorqueSM = application::FocStateMachineImpl<TorqueFoc, NoOp, AutoTransitionPolicy>;
        using SpeedSM = application::FocStateMachineImpl<SpeedFoc, NoOp, AutoTransitionPolicy>;
        using PositionSM = application::FocStateMachineImpl<PositionFoc, NoOp, AutoTransitionPolicy>;
        using ActiveSM = std::variant<std::monostate, TorqueSM, SpeedSM, PositionSM>;

        struct OuterLoopArgs
        {
            foc::Ampere maxCurrent;
            hal::Hertz baseFrequency;
            foc::LowPriorityInterrupt& lowPriorityInterrupt;
        };

        ControlModeStateMachine(
            const application::TerminalAndTracer& terminalAndTracer,
            const application::MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const application::CalibrationServices& calibServices,
            FaultNotifier& faultNotifier,
            const services::ConfigData& configData,
            OuterLoopArgs outerLoopArgs);

        void Select(ControlMode mode, infra::Function<void(SelectResult)> onDone);

        ControlMode Active() const;
        FocStateMachineBase& ActiveStateMachine();

        bool TrySetTorque(foc::IdAndIqPoint setpoint);
        bool TrySetSpeed(foc::RadiansPerSecond setpoint);
        bool TrySetPosition(foc::Radians setpoint);

    private:
        bool IsMotorStopped() const;
        void Activate(ControlMode mode);
        void RegisterCliCommands();
        void OnSaveConfigDone(services::NvmStatus status);

        const application::TerminalAndTracer terminalAndTracer;
        const application::MotorHardware hardware;
        services::NonVolatileMemory& nvm;
        const application::CalibrationServices calibServices;
        FaultNotifier& faultNotifier;
        OuterLoopArgs outerLoopArgs;

        services::ConfigData configData;
        ControlMode pendingSelectMode{ ControlMode::torque };
        infra::Function<void(SelectResult)> pendingSelectCallback;
        ActiveSM activeSm;
    };

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>::ControlModeStateMachine(
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

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    void ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>::Select(
        ControlMode mode,
        infra::Function<void(SelectResult)> onDone)
    {
        if (!IsMotorStopped())
        {
            onDone(SelectResult::busy);
            return;
        }

        configData.defaultControlMode = static_cast<uint8_t>(mode);
        pendingSelectMode = mode;
        pendingSelectCallback = onDone;
        nvm.SaveConfig(configData, [this](services::NvmStatus status)
            {
                OnSaveConfigDone(status);
            });
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    void ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>::OnSaveConfigDone(services::NvmStatus status)
    {
        auto callback = pendingSelectCallback;
        pendingSelectCallback = nullptr;
        if (status != services::NvmStatus::Ok)
        {
            callback(SelectResult::nvmFailed);
            return;
        }
        Activate(pendingSelectMode);
        callback(SelectResult::ok);
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    ControlMode ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>::Active() const
    {
        if (std::holds_alternative<SpeedSM>(activeSm))
            return ControlMode::speed;
        if (std::holds_alternative<PositionSM>(activeSm))
            return ControlMode::position;
        return ControlMode::torque;
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    FocStateMachineBase& ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>::ActiveStateMachine()
    {
        if (auto* sm = std::get_if<SpeedSM>(&activeSm))
            return *sm;
        if (auto* sm = std::get_if<PositionSM>(&activeSm))
            return *sm;
        if (auto* sm = std::get_if<TorqueSM>(&activeSm))
            return *sm;
        // Unreachable after construction
        return std::get<TorqueSM>(activeSm);
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    bool ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>::TrySetTorque(foc::IdAndIqPoint setpoint)
    {
        auto* sm = std::get_if<TorqueSM>(&activeSm);
        if (sm == nullptr)
            return false;
        sm->GetFoc().SetPoint(setpoint);
        return true;
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    bool ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>::TrySetSpeed(foc::RadiansPerSecond setpoint)
    {
        auto* sm = std::get_if<SpeedSM>(&activeSm);
        if (sm == nullptr)
            return false;
        sm->GetFoc().SetPoint(setpoint);
        return true;
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    bool ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>::TrySetPosition(foc::Radians setpoint)
    {
        auto* sm = std::get_if<PositionSM>(&activeSm);
        if (sm == nullptr)
            return false;
        sm->GetFoc().SetPoint(setpoint);
        return true;
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    bool ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>::IsMotorStopped() const
    {
        auto isStopped = [](const auto& sm) -> bool
        {
            return std::holds_alternative<Idle>(sm.CurrentState()) ||
                   std::holds_alternative<Ready>(sm.CurrentState());
        };

        if (const auto* sm = std::get_if<TorqueSM>(&activeSm))
            return isStopped(*sm);
        if (const auto* sm = std::get_if<SpeedSM>(&activeSm))
            return isStopped(*sm);
        if (const auto* sm = std::get_if<PositionSM>(&activeSm))
            return isStopped(*sm);
        return true;
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    void ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>::Activate(ControlMode mode)
    {
        switch (mode)
        {
            case ControlMode::torque:
                activeSm.template emplace<TorqueSM>(
                    terminalAndTracer,
                    hardware,
                    nvm,
                    calibServices,
                    faultNotifier);
                break;
            case ControlMode::speed:
                activeSm.template emplace<SpeedSM>(
                    terminalAndTracer,
                    hardware,
                    nvm,
                    calibServices,
                    faultNotifier,
                    outerLoopArgs.maxCurrent,
                    outerLoopArgs.baseFrequency,
                    outerLoopArgs.lowPriorityInterrupt);
                break;
            case ControlMode::position:
                activeSm.template emplace<PositionSM>(
                    terminalAndTracer,
                    hardware,
                    nvm,
                    calibServices,
                    faultNotifier,
                    outerLoopArgs.maxCurrent,
                    outerLoopArgs.baseFrequency,
                    outerLoopArgs.lowPriorityInterrupt);
                break;
            default:
                activeSm.template emplace<TorqueSM>(
                    terminalAndTracer,
                    hardware,
                    nvm,
                    calibServices,
                    faultNotifier);
                break;
        }
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    void ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>::RegisterCliCommands()
    {
        auto& terminal = terminalAndTracer.terminal;

        terminal.AddCommand({ { "calibrate", "cal", "Run full calibration sequence" },
            [this](const infra::BoundedConstString&)
            {
                ActiveStateMachine().CmdCalibrate();
            } });

        terminal.AddCommand({ { "enable", "en", "Enable FOC controller" },
            [this](const infra::BoundedConstString&)
            {
                ActiveStateMachine().CmdEnable();
            } });

        terminal.AddCommand({ { "disable", "dis", "Disable FOC controller" },
            [this](const infra::BoundedConstString&)
            {
                ActiveStateMachine().CmdDisable();
            } });

        terminal.AddCommand({ { "clear_fault", "cf", "Clear fault and return to Idle" },
            [this](const infra::BoundedConstString&)
            {
                ActiveStateMachine().CmdClearFault();
            } });

        terminal.AddCommand({ { "clear_cal", "cc", "Clear calibration data from NVM" },
            [this](const infra::BoundedConstString&)
            {
                ActiveStateMachine().CmdClearCalibration();
            } });

        terminal.AddCommand({ { "apply_estimates", "ae", "Apply online estimates to PID gains" },
            [this](const infra::BoundedConstString&)
            {
                if (Active() != ControlMode::torque)
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
