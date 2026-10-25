#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include "core/state_machine/ControlModeCliCommands.hpp"
#include "core/state_machine/MotorParameterLimits.hpp"
#include "core/state_machine/OperatorTunings.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <cmath>
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
        , configWriter(nvm, this->configData, terminalAndTracer.tracer, nvmActivity)
        , algorithmPersistence(configWriter, this->configData, terminalAndTracer.tracer)
    {
        SanitizeTunings(this->configData);
        RegisterCliCommands();
        Activate(ControlModeFromRaw(configData.defaultControlMode));
    }

    void ControlModeStateMachine::Select(ControlMode mode, const infra::Function<void(SelectResult)>& onDone)
    {
        if (pendingSelectCallback != nullptr || configWriter.HasPendingPersist())
        {
            onDone(SelectResult::busy);
            return;
        }

        if (!CanActivatePendingSelect())
        {
            onDone(SelectResult::busy);
            return;
        }

        previousDefaultControlMode = configData.defaultControlMode;
        configData.defaultControlMode = static_cast<uint8_t>(mode);
        pendingSelectMode = mode;
        pendingSelectCallback = onDone;
        nvmActivity.Begin();
        nvm.SaveConfig(configData, [this](services::NvmStatus status)
            {
                OnSaveConfigDone(status);
            });
    }

    bool ControlModeStateMachine::CanActivatePendingSelect() const
    {
        return IsStopped(ActiveStateMachine().CurrentState()) && !ActiveStateMachine().HasPendingAsyncWork();
    }

    void ControlModeStateMachine::OnSaveConfigDone(services::NvmStatus status)
    {
        nvmActivity.End();

        if (status != services::NvmStatus::Ok)
        {
            configData.defaultControlMode = previousDefaultControlMode;
            pendingSelectCallback(status == services::NvmStatus::Busy ? SelectResult::busy : SelectResult::nvmFailed);
            return;
        }

        if (CanActivatePendingSelect())
        {
            Activate(pendingSelectMode);
            pendingSelectCallback(SelectResult::ok);
            return;
        }

        configData.defaultControlMode = previousDefaultControlMode;
        nvmActivity.Begin();
        nvm.SaveConfig(configData, [this](services::NvmStatus rollbackStatus)
            {
                OnRollbackSaveConfigDone(rollbackStatus);
            });
    }

    void ControlModeStateMachine::OnRollbackSaveConfigDone(services::NvmStatus)
    {
        nvmActivity.End();
        pendingSelectCallback(SelectResult::busy);
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

    void ControlModeStateMachine::ApplyOnlineEstimates()
    {
        const auto applied = ActiveStateMachine().ApplyOnlineEstimates();

        if (auto* tunable = CurrentTunable(); applied.electrical && tunable != nullptr)
            tunable->SetCurrentTunings(EffectiveCurrentTunings());

        if (auto* tunable = SpeedTunable(); applied.mechanics && tunable != nullptr && tunable->SetSpeedTunings(*EffectiveSpeedTunings()) != foc::SelectResult::ok)
            terminalAndTracer.tracer.Trace() << "[SM] Speed settings not applied: the active law cannot be designed with them";
    }

    TuningResult ControlModeStateMachine::CheckRedesignPreconditions() const
    {
        if (std::holds_alternative<std::monostate>(activeSm))
            return TuningResult::wrongMode;

        if (!IsStopped(ActiveStateMachine().CurrentState()))
            return TuningResult::notStopped;

        return TuningResult::ok;
    }

    TuningResult ControlModeStateMachine::TrySetCurrentBandwidth(float bandwidth)
    {
        if (!foc::IsAcceptableCurrentBandwidth(bandwidth))
            return TuningResult::outOfRange;

        if (const auto rejection = CheckRedesignPreconditions(); rejection != TuningResult::ok)
            return rejection;

        auto tunings = StoredCurrentTunings();
        tunings.bandwidth = bandwidth;

        StoreTunings(configData, tunings);
        CurrentTunable()->SetCurrentTunings(EffectiveCurrentTunings());
        configWriter.Persist();
        return TuningResult::ok;
    }

    TuningResult ControlModeStateMachine::TrySetSpeedBandwidth(float bandwidth)
    {
        if (!foc::IsAcceptableSpeedBandwidth(bandwidth))
            return TuningResult::outOfRange;

        if (const auto rejection = CheckRedesignPreconditions(); rejection != TuningResult::ok)
            return rejection;

        auto* tunable = SpeedTunable();
        if (tunable == nullptr)
            return TuningResult::wrongMode;

        auto tunings = StoredSpeedTunings();
        tunings.bandwidth = bandwidth;
        if (tunable->SetSpeedTunings(tunings) != foc::SelectResult::ok)
            return TuningResult::outOfRange;

        StoreTunings(configData, tunings);
        configWriter.Persist();
        return TuningResult::ok;
    }

    TuningResult ControlModeStateMachine::TrySetPositionBandwidth(float bandwidth)
    {
        if (!foc::IsAcceptablePositionBandwidth(bandwidth))
            return TuningResult::outOfRange;

        if (const auto rejection = CheckRedesignPreconditions(); rejection != TuningResult::ok)
            return rejection;

        auto* tunable = PositionTunable();
        if (tunable == nullptr)
            return TuningResult::wrongMode;

        auto tunings = StoredPositionTunings();
        tunings.bandwidth = bandwidth;
        if (tunable->SetPositionTunings(tunings) != foc::SelectResult::ok)
            return TuningResult::outOfRange;

        StoreTunings(configData, tunings);
        configWriter.Persist();
        return TuningResult::ok;
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
                outerLoopArgs,
                nvmActivity));
        else if (mode == ControlMode::position)
            AttachAlgorithmRestore(activeSm.emplace<application::PositionStateMachine>(
                terminalAndTracer,
                hardware,
                nvm,
                calibServices,
                faultNotifier,
                TransitionPolicy::Auto,
                outerLoopArgs,
                nvmActivity));
        else
            AttachAlgorithmRestore(activeSm.emplace<application::TorqueStateMachine>(
                terminalAndTracer,
                hardware,
                nvm,
                calibServices,
                faultNotifier,
                TransitionPolicy::Auto,
                nvmActivity));
    }

    void ControlModeStateMachine::AttachAlgorithmRestore(application::FocStateMachineCommon& stateMachine)
    {
        stateMachine.RegisterReadyHandler([this]()
            {
                algorithmPersistence.ApplyPersistedAlgorithms(CurrentSelectable(), SpeedSelectable(), PositionSelectable());
                ApplyOperatorTunings();
            });

        if (std::holds_alternative<Ready>(stateMachine.CurrentState()))
        {
            algorithmPersistence.ApplyPersistedAlgorithms(CurrentSelectable(), SpeedSelectable(), PositionSelectable());
            ApplyOperatorTunings();
        }
    }

    foc::CurrentLoopTunable* ControlModeStateMachine::CurrentTunable()
    {
        if (auto* sm = std::get_if<application::TorqueStateMachine>(&activeSm))
            return &sm->GetController();
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return &sm->GetController();
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return &sm->GetController();
        return nullptr;
    }

    foc::SpeedLoopTunable* ControlModeStateMachine::SpeedTunable()
    {
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return &sm->GetController();
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return &sm->GetController();
        return nullptr;
    }

    foc::PositionLoopTunable* ControlModeStateMachine::PositionTunable()
    {
        if (auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return &sm->GetController();
        return nullptr;
    }

    const application::OuterLoopStateMachine* ControlModeStateMachine::ActiveOuterLoop() const
    {
        if (const auto* sm = std::get_if<application::SpeedStateMachine>(&activeSm))
            return sm;
        if (const auto* sm = std::get_if<application::PositionStateMachine>(&activeSm))
            return sm;
        return nullptr;
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
        if (!CanActivatePendingSelect())
            return foc::SelectResult::busy;
        return algorithmPersistence.SelectCurrentAlgorithm(algorithm, CurrentSelectable());
    }

    foc::SelectResult ControlModeStateMachine::SelectSpeedAlgorithm(foc::SpeedAlgorithm algorithm)
    {
        if (!CanActivatePendingSelect())
            return foc::SelectResult::busy;
        return algorithmPersistence.SelectSpeedAlgorithm(algorithm, SpeedSelectable());
    }

    foc::SelectResult ControlModeStateMachine::SelectPositionAlgorithm(foc::PositionAlgorithm algorithm)
    {
        if (!CanActivatePendingSelect())
            return foc::SelectResult::busy;
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

    CommandResult ControlModeStateMachine::CmdReserveExternalCalibration()
    {
        return ActiveCommon().CmdReserveExternalCalibration();
    }

    CommandResult ControlModeStateMachine::CmdReleaseExternalCalibration()
    {
        return ActiveCommon().CmdReleaseExternalCalibration();
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

    foc::CurrentLoopTunings ControlModeStateMachine::StoredCurrentTunings() const
    {
        return state_machine::StoredCurrentTunings(configData);
    }

    foc::SpeedLoopTunings ControlModeStateMachine::StoredSpeedTunings() const
    {
        return state_machine::StoredSpeedTunings(configData);
    }

    foc::PositionLoopTunings ControlModeStateMachine::StoredPositionTunings() const
    {
        return state_machine::StoredPositionTunings(configData);
    }

    foc::CurrentLoopTunings ControlModeStateMachine::EffectiveCurrentTunings() const
    {
        auto tunings = StoredCurrentTunings();
        if (tunings.bandwidth == 0.0f)
            tunings.bandwidth = ActiveCommon().AutomaticCurrentLoopBandwidth();
        return tunings;
    }

    std::optional<foc::SpeedLoopTunings> ControlModeStateMachine::EffectiveSpeedTunings() const
    {
        const auto* outerLoop = ActiveOuterLoop();
        if (outerLoop == nullptr)
            return std::nullopt;

        auto tunings = StoredSpeedTunings();
        if (tunings.bandwidth == 0.0f)
            tunings.bandwidth = outerLoop->AutomaticSpeedLoopBandwidth();
        return tunings;
    }

    std::optional<foc::PositionLoopTunings> ControlModeStateMachine::EffectivePositionTunings() const
    {
        if (Active() != ControlMode::position)
            return std::nullopt;

        auto tunings = StoredPositionTunings();
        if (tunings.bandwidth == 0.0f)
            tunings.bandwidth = foc::PositionLoopTunings{}.bandwidth;
        return tunings;
    }

    SettingResult ControlModeStateMachine::SettingGate() const
    {
        if (!IsStopped(ActiveStateMachine().CurrentState()))
            return SettingResult::invalidState;

        if (pendingSettingCallback != nullptr || pendingSelectCallback != nullptr || ActiveStateMachine().HasPendingAsyncWork() || configWriter.HasPendingPersist())
            return SettingResult::busy;

        return SettingResult::ok;
    }

    void ControlModeStateMachine::ApplyOperatorTunings()
    {
        if (auto* tunable = CurrentTunable())
            tunable->SetCurrentTunings(EffectiveCurrentTunings());

        if (auto* tunable = SpeedTunable(); tunable != nullptr && tunable->SetSpeedTunings(*EffectiveSpeedTunings()) != foc::SelectResult::ok)
            terminalAndTracer.tracer.Trace() << "[SM] Speed settings not applied: the active law cannot be designed with them";

        if (auto* tunable = PositionTunable(); tunable != nullptr && tunable->SetPositionTunings(*EffectivePositionTunings()) != foc::SelectResult::ok)
            terminalAndTracer.tracer.Trace() << "[SM] Position settings not applied: the active law cannot be designed with them";
    }

    void ControlModeStateMachine::StoreAndPersist(const OnSetting& onDone)
    {
        pendingSettingCallback = onDone;
        configWriter.Persist([this](services::NvmStatus status)
            {
                CompleteSetting(status == services::NvmStatus::Ok ? SettingResult::ok : SettingResult::nvmFailed);
            });
    }

    void ControlModeStateMachine::CompleteSetting(SettingResult result)
    {
        auto onDone = pendingSettingCallback;
        pendingSettingCallback = nullptr;
        onDone(result);
    }

    SettingResult ControlModeStateMachine::ToSettingResult(CommandResult result)
    {
        switch (result)
        {
            case CommandResult::nvmFailed:
                return SettingResult::nvmFailed;
            case CommandResult::abortedByFault:
                return SettingResult::aborted;
            default:
                return SettingResult::busy;
        }
    }

    SettingResult ControlModeStateMachine::ToSettingResult(foc::SelectResult result)
    {
        switch (result)
        {
            case foc::SelectResult::ok:
                return SettingResult::ok;
            case foc::SelectResult::busy:
                return SettingResult::busy;
            case foc::SelectResult::invalidAlgorithm:
                return SettingResult::wrongMode;
            default:
                return SettingResult::invalidParameters;
        }
    }

    void ControlModeStateMachine::SetCurrentTunings(const foc::CurrentLoopTunings& tunings, const OnSetting& onDone)
    {
        if (!IsAcceptable(tunings))
            onDone(SettingResult::outOfRange);
        else if (!IsDesignable(tunings))
            onDone(SettingResult::invalidParameters);
        else if (const auto gate = SettingGate(); gate != SettingResult::ok)
            onDone(gate);
        else
        {
            StoreTunings(configData, tunings);
            CurrentTunable()->SetCurrentTunings(EffectiveCurrentTunings());
            StoreAndPersist(onDone);
        }
    }

    void ControlModeStateMachine::SetSpeedTunings(const foc::SpeedLoopTunings& tunings, const OnSetting& onDone)
    {
        if (!IsAcceptable(tunings))
            onDone(SettingResult::outOfRange);
        else if (const auto gate = SettingGate(); gate != SettingResult::ok)
            onDone(gate);
        else if (SpeedTunable() == nullptr)
            onDone(SettingResult::wrongMode);
        else
        {
            const auto previous = StoredSpeedTunings();
            StoreTunings(configData, tunings);

            if (SpeedTunable()->SetSpeedTunings(*EffectiveSpeedTunings()) != foc::SelectResult::ok)
            {
                StoreTunings(configData, previous);
                onDone(SettingResult::invalidParameters);
                return;
            }

            StoreAndPersist(onDone);
        }
    }

    void ControlModeStateMachine::SetPositionTunings(const foc::PositionLoopTunings& tunings, const OnSetting& onDone)
    {
        if (!IsAcceptable(tunings))
            onDone(SettingResult::outOfRange);
        else if (const auto gate = SettingGate(); gate != SettingResult::ok)
            onDone(gate);
        else if (PositionTunable() == nullptr)
            onDone(SettingResult::wrongMode);
        else
        {
            const auto previous = StoredPositionTunings();
            StoreTunings(configData, tunings);

            if (const auto result = PositionTunable()->SetPositionTunings(*EffectivePositionTunings()); result != foc::SelectResult::ok)
            {
                StoreTunings(configData, previous);
                onDone(ToSettingResult(result));
                return;
            }

            StoreAndPersist(onDone);
        }
    }

    void ControlModeStateMachine::SelectCurrentAlgorithm(foc::CurrentAlgorithm algorithm, const OnSetting& onDone)
    {
        if (const auto gate = SettingGate(); gate != SettingResult::ok)
        {
            onDone(gate);
            return;
        }

        pendingSettingCallback = onDone;
        const auto result = algorithmPersistence.SelectCurrentAlgorithm(algorithm, CurrentSelectable(), [this](services::NvmStatus status)
            {
                CompleteSetting(status == services::NvmStatus::Ok ? SettingResult::ok : SettingResult::nvmFailed);
            });

        if (result != foc::SelectResult::ok)
            CompleteSetting(ToSettingResult(result));
    }

    void ControlModeStateMachine::SelectSpeedAlgorithm(foc::SpeedAlgorithm algorithm, const OnSetting& onDone)
    {
        if (const auto gate = SettingGate(); gate != SettingResult::ok)
        {
            onDone(gate);
            return;
        }

        pendingSettingCallback = onDone;
        const auto result = algorithmPersistence.SelectSpeedAlgorithm(algorithm, SpeedSelectable(), [this](services::NvmStatus status)
            {
                CompleteSetting(status == services::NvmStatus::Ok ? SettingResult::ok : SettingResult::nvmFailed);
            });

        if (result != foc::SelectResult::ok)
            CompleteSetting(ToSettingResult(result));
    }

    void ControlModeStateMachine::SelectPositionAlgorithm(foc::PositionAlgorithm algorithm, const OnSetting& onDone)
    {
        if (const auto gate = SettingGate(); gate != SettingResult::ok)
        {
            onDone(gate);
            return;
        }

        pendingSettingCallback = onDone;
        const auto result = algorithmPersistence.SelectPositionAlgorithm(algorithm, PositionSelectable(), [this](services::NvmStatus status)
            {
                CompleteSetting(status == services::NvmStatus::Ok ? SettingResult::ok : SettingResult::nvmFailed);
            });

        if (result != foc::SelectResult::ok)
            CompleteSetting(ToSettingResult(result));
    }

    void ControlModeStateMachine::SetMotorParameter(services::MotorParameter parameter, float value, const OnSetting& onDone)
    {
        if (!IsAcceptableMotorParameter(parameter, value))
        {
            onDone(SettingResult::outOfRange);
            return;
        }

        if (const auto gate = SettingGate(); gate != SettingResult::ok)
        {
            onDone(gate);
            return;
        }

        pendingSettingCallback = onDone;
        ActiveCommon().CmdSetMotorParameter(parameter, value, [this](CommandResult result)
            {
                if (result != CommandResult::ok)
                {
                    CompleteSetting(ToSettingResult(result));
                    return;
                }

                ApplyOperatorTunings();
                CompleteSetting(SettingResult::ok);
            });
    }

    const services::CalibrationData& ControlModeStateMachine::CalibrationRecord() const
    {
        return ActiveCommon().CalibrationRecord();
    }

    foc::Weber ControlModeStateMachine::DefaultFluxLinkage() const
    {
        return calibServices.fluxLinkage;
    }

    services::ConfigData& ControlModeStateMachine::Config()
    {
        return configData;
    }

    const services::ConfigData& ControlModeStateMachine::Config() const
    {
        return configData;
    }

    void ControlModeStateMachine::PersistConfig(const infra::Function<void(services::NvmStatus)>& onDone)
    {
        configWriter.Persist(onDone);
    }

    bool ControlModeStateMachine::HasPendingConfigWrite() const
    {
        return pendingSelectCallback != nullptr || configWriter.HasPendingPersist();
    }

    bool ControlModeStateMachine::HasPendingConfigPersist() const
    {
        return configWriter.HasPendingPersist();
    }

}
