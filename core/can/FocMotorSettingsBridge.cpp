#include "core/can/FocMotorSettingsBridge.hpp"
#include "core/can/FocMotorSettings.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include "core/services/cogging_compensation/CoggingMap.hpp"

namespace can
{
    namespace
    {
        uint32_t LoopStatistic(const application::ControlLoopMetrics::Snapshot& statistics, FocParameter parameter)
        {
            switch (parameter)
            {
                case FocParameter::loopSamples:
                    return statistics.samples;
                case FocParameter::loopBudgetCycles:
                    return statistics.budgetCycles;
                case FocParameter::loopMinimumCycles:
                    return statistics.minimumCycles;
                case FocParameter::loopAverageCycles:
                    return statistics.averageCycles;
                case FocParameter::loopMaximumCycles:
                    return statistics.maximumCycles;
                case FocParameter::loopOverruns:
                    return statistics.overruns;
                case FocParameter::loopDeadlineMisses:
                    return statistics.deadlineMisses;
                default:
                    return statistics.reentries;
            }
        }

        float StoredSettingOf(const services::ConfigData& config, FocParameter parameter)
        {
            if (GroupOf(parameter) == ParameterGroup::supervision)
                return SupervisionSettingOf(config, parameter);
            return static_cast<float>(StartupSettingOf(config, parameter));
        }

        void WriteStoredSetting(services::ConfigData& config, FocParameter parameter, float value)
        {
            if (GroupOf(parameter) == ParameterGroup::supervision)
                WriteSupervisionSetting(config, parameter, value);
            else
                WriteStartupSetting(config, parameter, static_cast<uint32_t>(value));
        }
    }

    FocMotorSettingsBridge::FocMotorSettingsBridge(FocMotorCategoryServer& server, state_machine::ControlModeStateMachine& controlMode,
        application::PlatformDiagnostics& diagnostics, FocResetCause resetCause)
        : server(server)
        , controlMode(controlMode)
        , diagnostics(diagnostics)
        , resetCause(resetCause)
    {}

    void FocMotorSettingsBridge::SetLegacyBandwidth(uint8_t messageId, FocParameter parameter, float bandwidth, const infra::Function<void()>& onDone)
    {
        if (SettingOutstanding())
        {
            server.SendCategoryError(messageId, FocMotorCategoryError::busy);
            return;
        }

        if (!(bandwidth >= foc::CommandLimits::minBandwidth))
        {
            server.SendCommandAck(messageId, services::CanAckStatus::invalidPayload);
            return;
        }

        pendingLegacyCommand = messageId;
        pendingNvmDoneCallback = onDone;
        SetLoopSetting(parameter, bandwidth, [this](state_machine::SettingResult result)
            {
                CompleteStoredCommand(pendingLegacyCommand, result);
            });
    }

    void FocMotorSettingsBridge::SetEncoderResolution(uint32_t resolution, const infra::Function<void()>& onDone)
    {
        if (SettingOutstanding())
        {
            server.SendCategoryError(can::focSetEncoderResolutionId, FocMotorCategoryError::busy);
            return;
        }

        if (resolution == 0)
        {
            server.SendCommandAck(can::focSetEncoderResolutionId, services::CanAckStatus::invalidPayload);
            return;
        }

        controlMode.Config().encoderResolution = resolution;
        pendingNvmDoneCallback = onDone;

        controlMode.PersistConfig([this](services::NvmStatus status)
            {
                auto callback = pendingNvmDoneCallback;
                pendingNvmDoneCallback = nullptr;
                if (status == services::NvmStatus::Ok)
                    callback();
                else
                    server.SendCategoryError(can::focSetEncoderResolutionId, FocMotorCategoryError::persistenceFailed);
            });
    }

    void FocMotorSettingsBridge::ConfigureTelemetryRate(uint32_t rateHz, const infra::Function<void()>& onDone)
    {
        if (SettingOutstanding())
        {
            server.SendCategoryError(can::focConfigureTelemetryRateId, FocMotorCategoryError::busy);
            return;
        }

        if (rateHz > focMaxTelemetryRateHz)
        {
            server.SendCommandAck(can::focConfigureTelemetryRateId, services::CanAckStatus::invalidPayload);
            return;
        }

        controlMode.Config().telemetryRateHz = rateHz;
        pendingNvmDoneCallback = onDone;

        controlMode.PersistConfig([this](services::NvmStatus status)
            {
                auto callback = pendingNvmDoneCallback;
                pendingNvmDoneCallback = nullptr;
                if (status == services::NvmStatus::Ok)
                    callback();
                else
                    server.SendCategoryError(can::focConfigureTelemetryRateId, FocMotorCategoryError::persistenceFailed);
            });
    }

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
            controlMode.SelectCurrentAlgorithm(ToCurrentAlgorithm(algorithm), completed);
        else if (loop == FocControlLoop::speed)
            controlMode.SelectSpeedAlgorithm(ToSpeedAlgorithm(algorithm), completed);
        else
            controlMode.SelectPositionAlgorithm(ToPositionAlgorithm(algorithm), completed);
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
            controlMode.SetMotorParameter(ToMotorParameter(parameter), value, completed);
        else if (GroupOf(parameter) == ParameterGroup::startup || GroupOf(parameter) == ParameterGroup::supervision)
            SetStoredSetting(parameter, value);
        else
            SetLoopSetting(parameter, value, completed);
    }

    FocAlgorithms FocMotorSettingsBridge::ActiveAlgorithms() const
    {
        return FocAlgorithms{
            state_machine::ToCanMode(controlMode.Active()),
            ToCanAlgorithm(controlMode.ActiveCurrentAlgorithm()),
            ToCanAlgorithm(controlMode.ActiveSpeedAlgorithm()),
            ToCanAlgorithm(controlMode.ActivePositionAlgorithm())
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
                return FocParameterValue{ StoredSettingOf(controlMode.Config(), parameter), false };
            case ParameterGroup::calibrationRecord:
                return HeldCalibrationReading(parameter);
            case ParameterGroup::onlineEstimate:
                return HeldOnlineEstimate(parameter);
            case ParameterGroup::loopStatistics:
                return HeldLoopStatistic(parameter);
            case ParameterGroup::platform:
                return FocParameterValue{ static_cast<float>(resetCause), false };
            default:
                return HeldLoopSetting(parameter);
        }
    }

    void FocMotorSettingsBridge::ClearDiagnostics()
    {
        diagnostics.Reset();
    }

    FocParameterValue FocMotorSettingsBridge::HeldCalibrationReading(FocParameter parameter) const
    {
        const auto& record = controlMode.CalibrationRecord();

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
        const auto estimates = controlMode.ActiveStateMachine().CurrentOnlineEstimates();
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

    FocParameterValue FocMotorSettingsBridge::HeldLoopStatistic(FocParameter parameter) const
    {
        return FocParameterValue{ static_cast<float>(LoopStatistic(diagnostics.ControlLoopStatistics(), parameter)), false };
    }

    bool FocMotorSettingsBridge::SettingOutstanding() const
    {
        return pendingNvmDoneCallback != nullptr || pendingAlgorithmCallback != nullptr || pendingParameterCallback != nullptr || controlMode.HasPendingConfigWrite();
    }

    void FocMotorSettingsBridge::SetLoopSetting(FocParameter parameter, float value, const state_machine::ControlModeStateMachine::OnSetting& completed)
    {
        if (GroupOf(parameter) == ParameterGroup::currentLoop)
            controlMode.SetCurrentTunings(WithSetting(controlMode.StoredCurrentTunings(), parameter, value), completed);
        else if (GroupOf(parameter) == ParameterGroup::speedLoop)
            controlMode.SetSpeedTunings(WithSetting(controlMode.StoredSpeedTunings(), parameter, value), completed);
        else
            controlMode.SetPositionTunings(WithSetting(controlMode.StoredPositionTunings(), parameter, value), completed);
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

    void FocMotorSettingsBridge::CompleteStoredCommand(uint8_t commandId, state_machine::SettingResult result)
    {
        auto onDone = pendingNvmDoneCallback;
        pendingNvmDoneCallback = nullptr;

        if (result == state_machine::SettingResult::ok)
            onDone();
        else
            ReportSettingFailure(commandId, result);
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
            return FocParameterValue{ SettingOf(controlMode.EffectiveCurrentTunings(), parameter),
                IsBandwidth(parameter) && controlMode.StoredCurrentTunings().bandwidth == 0.0f };

        if (GroupOf(parameter) == ParameterGroup::speedLoop)
        {
            const auto stored = controlMode.StoredSpeedTunings();
            return FocParameterValue{ SettingOf(controlMode.EffectiveSpeedTunings().value_or(stored), parameter), IsBandwidth(parameter) && stored.bandwidth == 0.0f };
        }

        const auto stored = controlMode.StoredPositionTunings();
        return FocParameterValue{ SettingOf(controlMode.EffectivePositionTunings().value_or(stored), parameter), IsBandwidth(parameter) && stored.bandwidth == 0.0f };
    }

    FocParameterValue FocMotorSettingsBridge::HeldMotorParameter(FocParameter parameter) const
    {
        const auto& record = controlMode.CalibrationRecord();

        if (parameter == FocParameter::fluxLinkage && !foc::IsFinitePositive(record.fluxLinkage))
            return FocParameterValue{ controlMode.DefaultFluxLinkage().Value(), true };

        return FocParameterValue{ services::ReadMotorParameter(record, ToMotorParameter(parameter)), false };
    }
}
