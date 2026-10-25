#include "core/can/FocMotorSettingsBridge.hpp"
#include "core/can/FocMotorSettings.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/foc/math/ParameterValidation.hpp"

namespace can
{
    FocMotorSettingsBridge::FocMotorSettingsBridge(FocMotorCategoryServer& server, state_machine::ControlModeStateMachine& controlMode)
        : server(server)
        , controlMode(controlMode)
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

        if (rateHz == 0 || rateHz > 10000)
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
        else if (GroupOf(parameter) == ParameterGroup::startup)
            SetStartupSetting(parameter, static_cast<uint32_t>(value));
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
                return FocParameterValue{ static_cast<float>(StartupSettingOf(controlMode.Config(), parameter)), false };
            default:
                return HeldLoopSetting(parameter);
        }
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

    void FocMotorSettingsBridge::SetStartupSetting(FocParameter parameter, uint32_t value)
    {
        previousStartupSetting = StartupSettingOf(controlMode.Config(), parameter);
        WriteStartupSetting(controlMode.Config(), parameter, value);

        controlMode.PersistConfig([this](services::NvmStatus status)
            {
                if (status != services::NvmStatus::Ok)
                    WriteStartupSetting(controlMode.Config(), pendingParameter, previousStartupSetting);

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
