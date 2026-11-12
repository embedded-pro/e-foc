#include "core/can/FocMotorSettingsBridge.hpp"
#include "core/can/FocMotorSettings.hpp"
#include "core/services/cogging_compensation/CoggingMap.hpp"
#include "e-foc-protocol/foc/ParameterValidation.hpp"
#include <limits>

namespace can
{
    namespace
    {
        uint32_t LoopStatistic(const infra::ExecutionTimeStatistics::Snapshot& statistics, FocParameter parameter)
        {
            switch (parameter)
            {
                case FocParameter::loopSamples:
                    return statistics.samples;
                case FocParameter::loopBudgetCycles:
                    return statistics.budget;
                case FocParameter::loopMinimumCycles:
                    return statistics.minimum;
                case FocParameter::loopAverageCycles:
                    return statistics.average;
                case FocParameter::loopMaximumCycles:
                    return statistics.maximum;
                case FocParameter::loopOverruns:
                    return statistics.overruns;
                case FocParameter::loopDeadlineMisses:
                    return statistics.deadlineMisses;
                default:
                    return statistics.reentries;
            }
        }

        bool IsRecalibrationReading(FocParameter parameter)
        {
            const auto id = static_cast<uint8_t>(parameter);
            return id >= static_cast<uint8_t>(FocParameter::modelResistance) && id <= static_cast<uint8_t>(FocParameter::recalibrationsStored);
        }

        bool IsStoredSetting(FocParameter parameter)
        {
            const auto group = GroupOf(parameter);
            return group == ParameterGroup::startup || group == ParameterGroup::supervision || group == ParameterGroup::limits;
        }

        float StoredSettingOf(const services::ConfigData& config, FocParameter parameter)
        {
            if (GroupOf(parameter) == ParameterGroup::supervision)
                return SupervisionSettingOf(config, parameter);
            if (GroupOf(parameter) == ParameterGroup::limits)
                return LimitSettingOf(config, parameter);
            return static_cast<float>(StartupSettingOf(config, parameter));
        }

        void WriteStoredSetting(services::ConfigData& config, FocParameter parameter, float value)
        {
            if (GroupOf(parameter) == ParameterGroup::supervision)
                WriteSupervisionSetting(config, parameter, value);
            else if (GroupOf(parameter) == ParameterGroup::limits)
                WriteLimitSetting(config, parameter, value);
            else
                WriteStartupSetting(config, parameter, static_cast<uint32_t>(value));
        }
    }

    FocMotorSettingsBridge::FocMotorSettingsBridge(FocMotorCategoryServer& server, state_machine::ControlModeStateMachine& controlMode,
        application::PlatformDiagnostics& diagnostics, FocResetCause resetCause, FocFirmwareIdentity firmware,
        FirmwareUpgrade firmwareUpgrade)
        : server(server)
        , controlMode(controlMode)
        , diagnostics(diagnostics)
        , resetCause(resetCause)
        , firmware(firmware)
        , firmwareUpgrade(firmwareUpgrade)
    {}

    void FocMotorSettingsBridge::SelectAlgorithm(FocControlLoop loop, uint8_t algorithm, const infra::Function<void(const FocAlgorithms&)>& onDone)
    {
        if (SettingOutstanding())
        {
            server.SendCategoryError(can::focSelectAlgorithmId, FocMotorCategoryError::busy);
            return;
        }

        pendingAlgorithmCallback = onDone;
        const auto completed = [this](state_machine::SettingResult result)
        {
            CompleteAlgorithmSelection(result);
        };

        if (loop == FocControlLoop::current)
            controlMode.Algorithms().SelectCurrentAlgorithm(ToCurrentAlgorithm(algorithm), completed);
        else if (loop == FocControlLoop::speed)
            controlMode.Algorithms().SelectSpeedAlgorithm(ToSpeedAlgorithm(algorithm), completed);
        else
            controlMode.Algorithms().SelectPositionAlgorithm(ToPositionAlgorithm(algorithm), completed);
    }

    void FocMotorSettingsBridge::SetParameter(FocParameter parameter, float value, const infra::Function<void(FocParameterValue)>& onDone)
    {
        if (SettingOutstanding())
        {
            server.SendCategoryError(can::focSetParameterId, FocMotorCategoryError::busy);
            return;
        }

        pendingParameter = parameter;
        pendingParameterCallback = onDone;

        const auto completed = [this](state_machine::SettingResult result)
        {
            CompleteParameter(result);
        };

        if (GroupOf(parameter) == ParameterGroup::motor)
            controlMode.Tunings().SetMotorParameter(ToMotorParameter(parameter), value, completed);
        else if (IsStoredSetting(parameter))
            SetStoredSetting(parameter, value);
        else
            SetLoopSetting(parameter, value, completed);
    }

    FocAlgorithms FocMotorSettingsBridge::ActiveAlgorithms() const
    {
        return FocAlgorithms{
            state_machine::ToCanMode(controlMode.Drive().Active()),
            ToCanAlgorithm(controlMode.Algorithms().ActiveCurrentAlgorithm()),
            ToCanAlgorithm(controlMode.Algorithms().ActiveSpeedAlgorithm()),
            ToCanAlgorithm(controlMode.Algorithms().ActivePositionAlgorithm())
        };
    }

    FocParameterValue FocMotorSettingsBridge::HeldValue(FocParameter parameter) const
    {
        switch (GroupOf(parameter))
        {
            case ParameterGroup::motor:
                return HeldMotorParameter(parameter);
            case ParameterGroup::startup:
            case ParameterGroup::supervision:
            case ParameterGroup::limits:
                return FocParameterValue{ StoredSettingOf(controlMode.Config(), parameter), false };
            case ParameterGroup::calibrationRecord:
                return HeldCalibrationReading(parameter);
            case ParameterGroup::onlineEstimate:
                return HeldOnlineEstimate(parameter);
            case ParameterGroup::loopStatistics:
                return HeldLoopStatistic(parameter);
            case ParameterGroup::platform:
                return HeldPlatformReading(parameter);
            case ParameterGroup::motion:
                return HeldMotionReading(parameter);
            case ParameterGroup::thermal:
                return HeldWindingTemperature();
            default:
                return HeldLoopSetting(parameter);
        }
    }

    void FocMotorSettingsBridge::ClearDiagnostics()
    {
        diagnostics.Reset();
    }

    void FocMotorSettingsBridge::ReportTargetReached(const infra::Function<bool()>& isReached)
    {
        targetReached = isReached;
    }

    FocParameterValue FocMotorSettingsBridge::HeldCalibrationReading(FocParameter parameter) const
    {
        const auto& record = controlMode.Drive().CalibrationRecord();

        switch (parameter)
        {
            case FocParameter::currentOffsetA:
                return FocParameterValue{ record.currentOffsetA, false };
            case FocParameter::currentOffsetB:
                return FocParameterValue{ record.currentOffsetB, false };
            case FocParameter::currentOffsetC:
                return FocParameterValue{ record.currentOffsetC, false };
            default:
                return FocParameterValue{ services::CoggingPeak(record.coggingTorque, record.coggingTorqueScale).Value(), false };
        }
    }

    FocParameterValue FocMotorSettingsBridge::HeldOnlineEstimate(FocParameter parameter) const
    {
        if (IsRecalibrationReading(parameter))
            return HeldRecalibrationReading(parameter);

        const auto estimates = controlMode.Drive().ActiveStateMachine().CurrentOnlineEstimates();
        if (!estimates.has_value())
            return FocParameterValue{ 0.0f, false };

        switch (parameter)
        {
            case FocParameter::estimatedResistance:
                return FocParameterValue{ estimates->resistance.Value(), false };
            case FocParameter::estimatedInductance:
                return FocParameterValue{ estimates->inductance.Value(), false };
            case FocParameter::estimatedInertia:
                return FocParameterValue{ estimates->inertia.Value(), false };
            default:
                return FocParameterValue{ estimates->viscousFriction.Value(), false };
        }
    }

    FocParameterValue FocMotorSettingsBridge::HeldRecalibrationReading(FocParameter parameter) const
    {
        if (parameter == FocParameter::recalibrationsApplied)
            return FocParameterValue{ static_cast<float>(controlMode.Recalibration().RecalibrationsApplied()), false };
        if (parameter == FocParameter::recalibrationsRejected)
            return FocParameterValue{ static_cast<float>(controlMode.Recalibration().RecalibrationsRejected()), false };
        if (parameter == FocParameter::recalibrationsStored)
            return FocParameterValue{ static_cast<float>(controlMode.Recalibration().RecalibrationsStored()), false };

        const auto model = controlMode.Recalibration().ModelInUse();
        if (!model.has_value())
            return FocParameterValue{ 0.0f, false };

        switch (parameter)
        {
            case FocParameter::modelResistance:
                return FocParameterValue{ model->resistance.Value(), false };
            case FocParameter::modelInductance:
                return FocParameterValue{ model->inductance.Value(), false };
            case FocParameter::modelInertia:
                return FocParameterValue{ model->inertia.Value(), false };
            default:
                return FocParameterValue{ model->viscousFriction.Value(), false };
        }
    }

    void FocMotorSettingsBridge::Recalibrate(const infra::Function<void()>& onAccepted)
    {
        const auto result = controlMode.Recalibration().CmdRecalibrate();
        if (result == state_machine::SettingResult::ok)
            onAccepted();
        else
            ReportSettingFailure(focRecalibrateId, result);
    }

    void FocMotorSettingsBridge::ResetModel(const infra::Function<void(bool stored)>& onDone)
    {
        const auto result = controlMode.Recalibration().CmdResetModel(onDone);
        if (result != state_machine::SettingResult::ok)
            ReportSettingFailure(focResetModelId, result);
    }

    FocParameterValue FocMotorSettingsBridge::HeldMotionReading(FocParameter parameter) const
    {
        const bool value = parameter == FocParameter::homed ? controlMode.Drive().IsHomed() : targetReached != nullptr && targetReached();
        return FocParameterValue{ value ? 1.0f : 0.0f, false };
    }

    FocParameterValue FocMotorSettingsBridge::HeldWindingTemperature() const
    {
        const auto temperature = controlMode.Drive().WindingTemperature();
        return FocParameterValue{ temperature.has_value() ? temperature->Value() : std::numeric_limits<float>::quiet_NaN(), false };
    }

    FocParameterValue FocMotorSettingsBridge::HeldLoopStatistic(FocParameter parameter) const
    {
        return FocParameterValue{ static_cast<float>(LoopStatistic(diagnostics.ControlLoopStatistics(), parameter)), false };
    }

    FocParameterValue FocMotorSettingsBridge::HeldPlatformReading(FocParameter parameter) const
    {
        const auto staged = firmwareUpgrade ? firmwareUpgrade->get().StagedVersion() : services::FirmwareVersion{};

        switch (parameter)
        {
            case FocParameter::firmwareVersionMajor:
                return FocParameterValue{ static_cast<float>(firmware.versionMajor), false };
            case FocParameter::firmwareVersionMinor:
                return FocParameterValue{ static_cast<float>(firmware.versionMinor), false };
            case FocParameter::firmwareVersionPatch:
                return FocParameterValue{ static_cast<float>(firmware.versionPatch), false };
            case FocParameter::boardKind:
                return FocParameterValue{ static_cast<float>(firmware.boardKind), false };
            case FocParameter::stagedVersionMajor:
                return FocParameterValue{ static_cast<float>(staged.major), false };
            case FocParameter::stagedVersionMinor:
                return FocParameterValue{ static_cast<float>(staged.minor), false };
            case FocParameter::stagedVersionPatch:
                return FocParameterValue{ static_cast<float>(staged.patch), false };
            default:
                return FocParameterValue{ static_cast<float>(resetCause), false };
        }
    }

    bool FocMotorSettingsBridge::SettingOutstanding() const
    {
        const bool downloading = firmwareUpgrade && firmwareUpgrade->get().IsRunning();
        return downloading || pendingAlgorithmCallback != nullptr || pendingParameterCallback != nullptr || controlMode.HasPendingConfigWrite();
    }

    void FocMotorSettingsBridge::SetLoopSetting(FocParameter parameter, float value, const state_machine::ControlModeStateMachine::OnSetting& completed)
    {
        if (GroupOf(parameter) == ParameterGroup::currentLoop)
            controlMode.Tunings().SetCurrentTunings(WithSetting(controlMode.Tunings().StoredCurrentTunings(), parameter, value), completed);
        else if (GroupOf(parameter) == ParameterGroup::speedLoop)
            controlMode.Tunings().SetSpeedTunings(WithSetting(controlMode.Tunings().StoredSpeedTunings(), parameter, value), completed);
        else
            controlMode.Tunings().SetPositionTunings(WithSetting(controlMode.Tunings().StoredPositionTunings(), parameter, value), completed);
    }

    void FocMotorSettingsBridge::SetStoredSetting(FocParameter parameter, float value)
    {
        previousStoredSetting = StoredSettingOf(controlMode.Config(), parameter);
        WriteStoredSetting(controlMode.Config(), parameter, value);

        controlMode.PersistConfig([this](services::NvmStatus status)
            {
                if (status != services::NvmStatus::Ok)
                    WriteStoredSetting(controlMode.Config(), pendingParameter, previousStoredSetting);

                CompleteParameter(status == services::NvmStatus::Ok ? state_machine::SettingResult::ok : state_machine::SettingResult::nvmFailed);
            });
    }

    void FocMotorSettingsBridge::CompleteAlgorithmSelection(state_machine::SettingResult result)
    {
        auto onDone = pendingAlgorithmCallback;
        pendingAlgorithmCallback = nullptr;

        if (result == state_machine::SettingResult::ok)
            onDone(ActiveAlgorithms());
        else
            ReportSettingFailure(can::focSelectAlgorithmId, result);
    }

    void FocMotorSettingsBridge::CompleteParameter(state_machine::SettingResult result)
    {
        auto onDone = pendingParameterCallback;
        pendingParameterCallback = nullptr;

        if (result == state_machine::SettingResult::ok)
            onDone(HeldValue(pendingParameter));
        else
            ReportSettingFailure(can::focSetParameterId, result);
    }

    void FocMotorSettingsBridge::ReportSettingFailure(uint8_t commandId, state_machine::SettingResult result)
    {
        switch (result)
        {
            case state_machine::SettingResult::invalidState:
                server.SendCommandAck(commandId, services::CanAckStatus::invalidState);
                break;
            case state_machine::SettingResult::outOfRange:
                server.SendCommandAck(commandId, services::CanAckStatus::invalidPayload);
                break;
            case state_machine::SettingResult::wrongMode:
                server.SendCategoryError(commandId, FocMotorCategoryError::modeMismatch);
                break;
            case state_machine::SettingResult::invalidParameters:
                server.SendCategoryError(commandId, FocMotorCategoryError::invalidParameters);
                break;
            case state_machine::SettingResult::nvmFailed:
                server.SendCategoryError(commandId, FocMotorCategoryError::persistenceFailed);
                break;
            case state_machine::SettingResult::aborted:
                server.SendCategoryError(commandId, FocMotorCategoryError::abortedByFault);
                break;
            default:
                server.SendCategoryError(commandId, FocMotorCategoryError::busy);
                break;
        }
    }

    FocParameterValue FocMotorSettingsBridge::HeldLoopSetting(FocParameter parameter) const
    {
        if (GroupOf(parameter) == ParameterGroup::currentLoop)
            return FocParameterValue{ SettingOf(controlMode.Tunings().EffectiveCurrentTunings(), parameter),
                IsBandwidth(parameter) && controlMode.Tunings().StoredCurrentTunings().bandwidth == 0.0f };

        if (GroupOf(parameter) == ParameterGroup::speedLoop)
        {
            const auto stored = controlMode.Tunings().StoredSpeedTunings();
            return FocParameterValue{ SettingOf(controlMode.Tunings().EffectiveSpeedTunings().value_or(stored), parameter), IsBandwidth(parameter) && stored.bandwidth == 0.0f };
        }

        const auto stored = controlMode.Tunings().StoredPositionTunings();
        return FocParameterValue{ SettingOf(controlMode.Tunings().EffectivePositionTunings().value_or(stored), parameter), IsBandwidth(parameter) && stored.bandwidth == 0.0f };
    }

    FocParameterValue FocMotorSettingsBridge::HeldMotorParameter(FocParameter parameter) const
    {
        const auto& record = controlMode.Drive().CalibrationRecord();

        if (parameter == FocParameter::fluxLinkage && !foc::IsFinitePositive(record.fluxLinkage))
            return FocParameterValue{ controlMode.DefaultFluxLinkage().Value(), true };

        return FocParameterValue{ services::ReadMotorParameter(record, ToMotorParameter(parameter)), false };
    }
}
