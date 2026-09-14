#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include "core/state_machine/ControlModeCliCommands.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <optional>

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
        RegisterControlModeCliCommands(terminalAndTracer.terminal, *this, terminalAndTracer.tracer);
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

}
