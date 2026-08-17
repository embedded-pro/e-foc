#include "core/state_machine/ControlModeStateMachine.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/controllers/interfaces/PidController.hpp"
#include <optional>

namespace
{
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

    const char* CurrentAlgorithmName(foc::CurrentAlgorithm algorithm)
    {
        switch (algorithm)
        {
            case foc::CurrentAlgorithm::pid:
                return "pid";
            case foc::CurrentAlgorithm::decoupledPid:
                return "decoupled";
            case foc::CurrentAlgorithm::deadbeat:
                return "deadbeat";
            case foc::CurrentAlgorithm::slidingMode:
                return "sliding";
            default:
                return "unknown";
        }
    }

    const char* SpeedAlgorithmName(foc::SpeedAlgorithm algorithm)
    {
        switch (algorithm)
        {
            case foc::SpeedAlgorithm::pid:
                return "pid";
            case foc::SpeedAlgorithm::lqi:
                return "lqi";
            case foc::SpeedAlgorithm::adrc:
                return "adrc";
            case foc::SpeedAlgorithm::twoDof:
                return "twodof";
            default:
                return "unknown";
        }
    }

    const char* PositionAlgorithmName(foc::PositionAlgorithm algorithm)
    {
        switch (algorithm)
        {
            case foc::PositionAlgorithm::pid:
                return "pid";
            case foc::PositionAlgorithm::cascadeP:
                return "cascadep";
            case foc::PositionAlgorithm::lqr:
                return "lqr";
            case foc::PositionAlgorithm::lqi:
                return "lqi";
            case foc::PositionAlgorithm::twoDof:
                return "twodof";
            default:
                return "unknown";
        }
    }

    // A CRC-valid record can still hold a byte outside the enum domain, so every persisted
    // selection is range-checked before it reaches a static_cast.
    std::optional<foc::CurrentAlgorithm> CurrentAlgorithmFromRaw(uint8_t raw)
    {
        if (raw > static_cast<uint8_t>(foc::CurrentAlgorithm::slidingMode))
            return std::nullopt;
        return static_cast<foc::CurrentAlgorithm>(raw);
    }

    std::optional<foc::SpeedAlgorithm> SpeedAlgorithmFromRaw(uint8_t raw)
    {
        if (raw > static_cast<uint8_t>(foc::SpeedAlgorithm::twoDof))
            return std::nullopt;
        return static_cast<foc::SpeedAlgorithm>(raw);
    }

    std::optional<foc::PositionAlgorithm> PositionAlgorithmFromRaw(uint8_t raw)
    {
        if (raw > static_cast<uint8_t>(foc::PositionAlgorithm::twoDof))
            return std::nullopt;
        return static_cast<foc::PositionAlgorithm>(raw);
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

    bool ControlModeStateMachine::TrySetCurrentBandwidth(float bandwidth)
    {
        auto tunings = foc::CurrentLoopTunings{};
        tunings.bandwidth = bandwidth;

        if (auto* sm = std::get_if<application::TorqueStateMachine>(&activeSm))
        {
            sm->GetController().SetCurrentTunings(tunings);
            return true;
        }
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
        {
            sm->GetController().SetCurrentTunings(tunings);
            return true;
        }
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
        {
            sm->GetController().SetCurrentTunings(tunings);
            return true;
        }
        return false;
    }

    bool ControlModeStateMachine::TrySetSpeedBandwidth(float bandwidth)
    {
        auto tunings = foc::SpeedLoopTunings{};
        tunings.bandwidth = bandwidth;

        if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
        {
            sm->GetController().SetSpeedTunings(tunings);
            return true;
        }
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
        {
            sm->GetController().SetSpeedTunings(tunings);
            return true;
        }
        return false;
    }

    bool ControlModeStateMachine::TrySetPositionBandwidth(float bandwidth)
    {
        auto* sm = std::get_if<application::PositionStateMachine>(&activeSm);
        if (sm == nullptr)
            return false;

        auto tunings = foc::PositionLoopTunings{};
        tunings.bandwidth = bandwidth;

        // The position law is redesigned on retuning, so a rejected design must not look accepted
        return sm->GetController().SetPositionTunings(tunings) == foc::SelectResult::ok;
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

        ApplyPersistedAlgorithms();
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
        return const_cast<ControlModeStateMachine*>(this)->CurrentSelectable();
    }

    const foc::SpeedLoopSelectable* ControlModeStateMachine::SpeedSelectable() const
    {
        return const_cast<ControlModeStateMachine*>(this)->SpeedSelectable();
    }

    const foc::PositionLoopSelectable* ControlModeStateMachine::PositionSelectable() const
    {
        return const_cast<ControlModeStateMachine*>(this)->PositionSelectable();
    }

    void ControlModeStateMachine::ApplyPersistedAlgorithms()
    {
        if (auto* selectable = CurrentSelectable())
        {
            const auto algorithm = CurrentAlgorithmFromRaw(configData.currentAlgorithm);
            if (!algorithm.has_value() || selectable->SelectCurrentAlgorithm(*algorithm) != foc::SelectResult::ok)
                configData.currentAlgorithm = static_cast<uint8_t>(selectable->ActiveCurrentAlgorithm());
        }

        if (auto* selectable = SpeedSelectable())
        {
            const auto algorithm = SpeedAlgorithmFromRaw(configData.speedAlgorithm);
            if (!algorithm.has_value() || selectable->SelectSpeedAlgorithm(*algorithm) != foc::SelectResult::ok)
                configData.speedAlgorithm = static_cast<uint8_t>(selectable->ActiveSpeedAlgorithm());
        }

        if (auto* selectable = PositionSelectable())
        {
            const auto algorithm = PositionAlgorithmFromRaw(configData.positionAlgorithm);
            if (!algorithm.has_value() || selectable->SelectPositionAlgorithm(*algorithm) != foc::SelectResult::ok)
                configData.positionAlgorithm = static_cast<uint8_t>(selectable->ActivePositionAlgorithm());
        }
    }

    void ControlModeStateMachine::PersistConfig()
    {
        nvm.SaveConfig(configData, [this](services::NvmStatus status)
            {
                if (status != services::NvmStatus::Ok)
                    terminalAndTracer.tracer.Trace() << "config persist failed";
            });
    }

    foc::SelectResult ControlModeStateMachine::SelectCurrentAlgorithm(foc::CurrentAlgorithm algorithm)
    {
        auto* selectable = CurrentSelectable();
        if (selectable == nullptr)
            return foc::SelectResult::invalidAlgorithm;

        auto result = selectable->SelectCurrentAlgorithm(algorithm);
        if (result != foc::SelectResult::ok)
            return result;

        configData.currentAlgorithm = static_cast<uint8_t>(algorithm);
        PersistConfig();
        return result;
    }

    foc::SelectResult ControlModeStateMachine::SelectSpeedAlgorithm(foc::SpeedAlgorithm algorithm)
    {
        auto* selectable = SpeedSelectable();
        if (selectable == nullptr)
            return foc::SelectResult::invalidAlgorithm;

        auto result = selectable->SelectSpeedAlgorithm(algorithm);
        if (result != foc::SelectResult::ok)
            return result;

        configData.speedAlgorithm = static_cast<uint8_t>(algorithm);
        PersistConfig();
        return result;
    }

    foc::SelectResult ControlModeStateMachine::SelectPositionAlgorithm(foc::PositionAlgorithm algorithm)
    {
        auto* selectable = PositionSelectable();
        if (selectable == nullptr)
            return foc::SelectResult::invalidAlgorithm;

        auto result = selectable->SelectPositionAlgorithm(algorithm);
        if (result != foc::SelectResult::ok)
            return result;

        configData.positionAlgorithm = static_cast<uint8_t>(algorithm);
        PersistConfig();
        return result;
    }

    foc::CurrentAlgorithm ControlModeStateMachine::ActiveCurrentAlgorithm() const
    {
        if (const auto* selectable = CurrentSelectable())
            return selectable->ActiveCurrentAlgorithm();

        return CurrentAlgorithmFromRaw(configData.currentAlgorithm).value_or(foc::CurrentAlgorithm::pid);
    }

    foc::SpeedAlgorithm ControlModeStateMachine::ActiveSpeedAlgorithm() const
    {
        if (const auto* selectable = SpeedSelectable())
            return selectable->ActiveSpeedAlgorithm();

        return SpeedAlgorithmFromRaw(configData.speedAlgorithm).value_or(foc::SpeedAlgorithm::pid);
    }

    foc::PositionAlgorithm ControlModeStateMachine::ActivePositionAlgorithm() const
    {
        if (const auto* selectable = PositionSelectable())
            return selectable->ActivePositionAlgorithm();

        return PositionAlgorithmFromRaw(configData.positionAlgorithm).value_or(foc::PositionAlgorithm::pid);
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
                terminalAndTracer.tracer.Trace() << "Current loop: " << CurrentAlgorithmName(ActiveCurrentAlgorithm());
                terminalAndTracer.tracer.Trace() << "Speed loop: " << SpeedAlgorithmName(ActiveSpeedAlgorithm());
                terminalAndTracer.tracer.Trace() << "Position loop: " << PositionAlgorithmName(ActivePositionAlgorithm());
            } });
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
