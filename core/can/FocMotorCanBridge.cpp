#include "core/can/FocMotorCanBridge.hpp"
#include "core/can/FocMotorSettings.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include "core/foc/math/TorqueConstant.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include <cmath>
#include <limits>
#include <variant>

namespace can
{
    FocMotorCanBridge::FocMotorCanBridge(
        FocMotorCategoryServer& server,
        state_machine::ControlModeStateMachine& controlMode,
        const drivers::ThreePhaseInverter& inverter,
        drivers::Encoder& encoder,
        services::ElectricalParametersIdentification& electricalIdent,
        services::MechanicalParametersIdentification* mechIdent,
        services::Tracer& tracer)
        : FocMotorCategoryServerObserver(server)
        , server(server)
        , controlMode(controlMode)
        , inverter(inverter)
        , encoder(encoder)
        , electricalIdent(electricalIdent)
        , mechIdent{ mechIdent }
    {
        tracer.Trace() << "FocMotorCanBridge: initialised";
    }

    void FocMotorCanBridge::BroadcastFault(state_machine::FaultCode code)
    {
        server.BroadcastFaultStatus(ToCanFaultCode(code));
    }

    FocFaultCode FocMotorCanBridge::ToCanFaultCode(state_machine::FaultCode code)
    {
        switch (code)
        {
            case state_machine::FaultCode::overcurrent:
                return FocFaultCode::overCurrent;
            case state_machine::FaultCode::overvoltage:
                return FocFaultCode::overVoltage;
            case state_machine::FaultCode::undervoltage:
                return FocFaultCode::underVoltage;
            case state_machine::FaultCode::overtemperature:
                return FocFaultCode::overTemperature;
            case state_machine::FaultCode::encoderLoss:
                return FocFaultCode::sensorFault;
            default:
                return FocFaultCode::none;
        }
    }

    FocMotorState FocMotorCanBridge::ToCanMotorState(const state_machine::State& state, bool hasPartialCalibration)
    {
        if (std::holds_alternative<state_machine::Fault>(state))
            return FocMotorState::fault;
        if (std::holds_alternative<state_machine::Calibrating>(state))
            return FocMotorState::calibrating;
        if (std::holds_alternative<state_machine::Enabled>(state))
            return FocMotorState::running;
        if (hasPartialCalibration)
            return FocMotorState::partialCalibration;
        return FocMotorState::idle;
    }

    void FocMotorCanBridge::OnStart(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        ReportCommandOutcome(can::focStartId, controlMode.ActiveStateMachine().CmdEnable(), onDone);
    }

    void FocMotorCanBridge::OnStop(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        ReportCommandOutcome(can::focStopId, controlMode.ActiveStateMachine().CmdDisable(), onDone);
    }

    void FocMotorCanBridge::OnClearFault(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        ReportCommandOutcome(can::focClearFaultId, controlMode.ActiveStateMachine().CmdClearFault(), onDone);
    }

    void FocMotorCanBridge::OnEmergencyStop(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        if (mechIdent != nullptr)
            mechIdent->Abort();

        ReportCommandOutcome(can::focEmergencyStopId, controlMode.ActiveStateMachine().CmdEmergencyStop(), onDone);
    }

    void FocMotorCanBridge::OnSelectControlMode(FocMotorMode mode,
        const infra::Function<void(FocMotorMode)>& onActivated)
    {
        if (pendingSelectCallback != nullptr)
        {
            server.SendCategoryError(can::focSelectControlModeId, FocMotorCategoryError::busy);
            return;
        }

        const auto mapped = state_machine::FromCanMode(mode);
        if (!mapped.has_value())
        {
            server.SendCommandAck(can::focSelectControlModeId, services::CanAckStatus::invalidPayload);
            return;
        }

        pendingSelectCallback = onActivated;
        controlMode.Select(*mapped, [this](state_machine::SelectResult result)
            {
                auto callback = pendingSelectCallback;
                pendingSelectCallback = nullptr;
                if (result == state_machine::SelectResult::ok)
                    callback(state_machine::ToCanMode(controlMode.Active()));
                else
                    ReportSelectFailure(result);
            });
    }

    void FocMotorCanBridge::OnSetTorqueSetpoint(foc::Ampere value, const infra::Function<void()>& onDone)
    {
        if (SetpointRejected(can::focSetTorqueSetpointId, state_machine::ControlMode::torque,
                std::fabs(value.Value()) <= inverter.MaxCurrentSupported().Value()))
            return;

        if (controlMode.TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, value }))
            onDone();
        else
            server.SendCategoryError(can::focSetTorqueSetpointId, FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnSetSpeedSetpoint(foc::RadiansPerSecond value, const infra::Function<void()>& onDone)
    {
        if (SetpointRejected(can::focSetSpeedSetpointId, state_machine::ControlMode::speed,
                std::fabs(value.Value()) <= maxSpeedSetpoint))
            return;

        if (controlMode.TrySetSpeed(value))
            onDone();
        else
            server.SendCategoryError(can::focSetSpeedSetpointId, FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnSetPositionSetpoint(foc::Radians value, const infra::Function<void()>& onDone)
    {
        if (SetpointRejected(can::focSetPositionSetpointId, state_machine::ControlMode::position,
                std::fabs(value.Value()) <= maxPositionSetpoint))
            return;

        if (controlMode.TrySetPosition(value))
            onDone();
        else
            server.SendCategoryError(can::focSetPositionSetpointId, FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnSetPidCurrent(float bandwidth, const infra::Function<void()>& onDone)
    {
        ReportTuningOutcome(can::focSetPidCurrentId, controlMode.TrySetCurrentBandwidth(bandwidth), onDone);
    }

    void FocMotorCanBridge::OnSetPidSpeed(float bandwidth, const infra::Function<void()>& onDone)
    {
        ReportTuningOutcome(can::focSetPidSpeedId, controlMode.TrySetSpeedBandwidth(bandwidth), onDone);
    }

    void FocMotorCanBridge::OnSetPidPosition(float bandwidth, const infra::Function<void()>& onDone)
    {
        ReportTuningOutcome(can::focSetPidPositionId, controlMode.TrySetPositionBandwidth(bandwidth), onDone);
    }

    void FocMotorCanBridge::ReportTuningOutcome(uint8_t messageId, state_machine::TuningResult result, const infra::Function<void()>& onDone)
    {
        if (result == state_machine::TuningResult::ok)
        {
            onDone();
            return;
        }

        // A redesign refused because the drive is running is a state problem, not a malformed frame
        server.SendCommandAck(messageId, result == state_machine::TuningResult::notWhileEnabled
                                             ? services::CanAckStatus::invalidState
                                             : services::CanAckStatus::invalidPayload);
    }

    void FocMotorCanBridge::OnAlign(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        if (pendingAlignDoneCallback != nullptr)
        {
            server.SendCategoryError(can::focAlignId, FocMotorCategoryError::busy);
            return;
        }

        pendingAlignDoneCallback = onDone;
        controlMode.CmdReAlign([this](state_machine::CommandResult result)
            {
                auto callback = pendingAlignDoneCallback;
                pendingAlignDoneCallback = nullptr;
                if (result == state_machine::CommandResult::ok)
                    callback(services::CanAckStatus::success);
                else
                    server.SendCategoryError(can::focAlignId, state_machine::ToCategoryError(result));
            });
    }

    bool FocMotorCanBridge::ReserveCalibration(uint8_t commandId)
    {
        const auto reservation = controlMode.CmdReserveExternalCalibration();

        if (reservation == state_machine::CommandResult::ok)
            return true;

        if (reservation == state_machine::CommandResult::queued)
            server.SendCategoryError(commandId, FocMotorCategoryError::busy);
        else
            server.SendCommandAck(commandId, services::CanAckStatus::invalidState);

        return false;
    }

    // An emergency stop or a fault ends the reservation and aborts the estimation without answering it
    bool FocMotorCanBridge::ElectricalIdentificationInProgress() const
    {
        return pendingElectricalIdentDoneCallback != nullptr && std::holds_alternative<state_machine::Calibrating>(controlMode.ActiveStateMachine().CurrentState());
    }

    bool FocMotorCanBridge::MechanicalIdentificationInProgress() const
    {
        return mechIdent->IsRunning() || (pendingMechIdentDoneCallback != nullptr && std::holds_alternative<state_machine::Calibrating>(controlMode.ActiveStateMachine().CurrentState()));
    }

    void FocMotorCanBridge::FailElectricalIdentification(FocMotorCategoryError error)
    {
        pendingElectricalR.reset();
        pendingElectricalL.reset();
        pendingElectricalIdentDoneCallback = nullptr;
        controlMode.CmdReleaseExternalCalibration();
        server.SendCategoryError(can::focIdentifyElectricalId, error);
    }

    void FocMotorCanBridge::OnIdentifyElectrical(const infra::Function<void()>& onDone)
    {
        if (ElectricalIdentificationInProgress())
        {
            server.SendCategoryError(can::focIdentifyElectricalId, FocMotorCategoryError::busy);
            return;
        }

        if (!ReserveCalibration(can::focIdentifyElectricalId))
            return;

        pendingElectricalR.reset();
        pendingElectricalL.reset();
        pendingElectricalIdentDoneCallback = onDone;

        electricalIdent.EstimateResistanceAndInductance({},
            [this](services::ElectricalParametersIdentification::ResistanceInductanceResult result)
            {
                if (!result.resistance.has_value() || !result.inductance.has_value() || result.fitQuality < 0.5f)
                {
                    FailElectricalIdentification(FocMotorCategoryError::calibrationFailed);
                    return;
                }

                pendingElectricalR = result.resistance;
                pendingElectricalL = result.inductance;

                electricalIdent.EstimateNumberOfPolePairs({},
                    [this](std::optional<std::size_t> polePairs)
                    {
                        if (!polePairs.has_value() || *polePairs > std::numeric_limits<uint8_t>::max())
                        {
                            FailElectricalIdentification(FocMotorCategoryError::calibrationFailed);
                            return;
                        }

                        services::CalibrationData data{};
                        data.rPhase = pendingElectricalR->Value();
                        data.lD = pendingElectricalL->Value();
                        data.lQ = pendingElectricalL->Value();
                        data.polePairs = static_cast<uint8_t>(*polePairs);
                        data.fluxLinkage = controlMode.ActiveFluxLinkage().Value();
                        pendingElectricalR.reset();
                        pendingElectricalL.reset();

                        server.BroadcastElectricalParams(
                            foc::Ohm{ data.rPhase },
                            foc::MilliHenry{ data.lD },
                            static_cast<std::size_t>(data.polePairs));

                        controlMode.CmdCompleteExternalCalibration(data,
                            [this](state_machine::CommandResult result)
                            {
                                auto callback = pendingElectricalIdentDoneCallback;
                                pendingElectricalIdentDoneCallback = nullptr;

                                if (result == state_machine::CommandResult::ok)
                                    callback();
                                else
                                {
                                    if (result == state_machine::CommandResult::rejected)
                                        controlMode.CmdReleaseExternalCalibration();

                                    server.SendCategoryError(can::focIdentifyElectricalId,
                                        state_machine::ToCategoryError(result));
                                }
                            });
                    });
            });
    }

    void FocMotorCanBridge::OnIdentifyMechanical(const infra::Function<void()>& onDone)
    {
        if (mechIdent == nullptr)
        {
            server.SendCommandAck(can::focIdentifyMechanicalId, services::CanAckStatus::notImplemented);
            return;
        }

        if (MechanicalIdentificationInProgress())
        {
            server.SendCategoryError(can::focIdentifyMechanicalId, FocMotorCategoryError::busy);
            return;
        }

        const auto cal = controlMode.ActiveCalibrationData();
        if (!cal.has_value())
        {
            server.SendCommandAck(can::focIdentifyMechanicalId, services::CanAckStatus::invalidState);
            return;
        }

        if (!ReserveCalibration(can::focIdentifyMechanicalId))
            return;

        pendingMechIdentDoneCallback = onDone;

        mechIdent->EstimateFrictionAndInertia(
            foc::TorqueConstantFor(cal->polePairs, controlMode.ActiveFluxLinkage()),
            static_cast<std::size_t>(cal->polePairs),
            {},
            [this](std::optional<foc::NewtonMeterSecondPerRadian> friction,
                std::optional<foc::NewtonMeterSecondSquared> inertia)
            {
                auto callback = pendingMechIdentDoneCallback;
                pendingMechIdentDoneCallback = nullptr;
                controlMode.CmdReleaseExternalCalibration();

                if (!friction.has_value() || !inertia.has_value())
                {
                    server.SendCategoryError(can::focIdentifyMechanicalId, FocMotorCategoryError::calibrationFailed);
                    return;
                }

                server.BroadcastMechanicalParams(*friction, *inertia);
                callback();
            });
    }

    void FocMotorCanBridge::OnRequestTelemetry(const infra::Function<void()>& onDone)
    {
        const auto& state = controlMode.ActiveStateMachine().CurrentState();
        const auto faultCode = std::holds_alternative<state_machine::Fault>(state)
                                   ? ToCanFaultCode(controlMode.ActiveStateMachine().LastFaultCode())
                                   : FocFaultCode::none;

        server.BroadcastTelemetryStatus(
            ToCanMotorState(state, controlMode.ActiveStateMachine().HasPartialCalibration()),
            faultCode,
            controlMode.ActiveStateMachine().MeasuredSpeed(),
            encoder.Read());
        onDone();
    }

    void FocMotorCanBridge::OnSetEncoderResolution(uint32_t resolution, const infra::Function<void()>& onDone)
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

    void FocMotorCanBridge::OnConfigureTelemetryRate(uint32_t rateHz, const infra::Function<void()>& onDone)
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

    bool FocMotorCanBridge::SettingOutstanding() const
    {
        return pendingNvmDoneCallback != nullptr || pendingAlgorithmCallback != nullptr || pendingParameterCallback != nullptr;
    }

    void FocMotorCanBridge::OnSelectAlgorithm(FocControlLoop loop, uint8_t algorithm, const infra::Function<void(const FocAlgorithms&)>& onDone)
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

    void FocMotorCanBridge::OnQueryAlgorithms(const infra::Function<void(const FocAlgorithms&)>& onDone)
    {
        onDone(ActiveAlgorithms());
    }

    void FocMotorCanBridge::OnSetParameter(FocParameter parameter, float value, const infra::Function<void(FocParameterValue)>& onDone)
    {
        if (SettingOutstanding())
        {
            server.SendCategoryError(can::focSetParameterId, FocMotorCategoryError::busy);
            return;
        }

        pendingParameter = parameter;
        pendingParameterCallback = onDone;

        if (GroupOf(parameter) == ParameterGroup::motor)
            controlMode.SetMotorParameter(ToMotorParameter(parameter), value, [this](state_machine::SettingResult result)
                {
                    CompleteParameter(result);
                });
        else if (GroupOf(parameter) == ParameterGroup::startup)
            SetStartupSetting(parameter, static_cast<uint32_t>(value));
        else
            SetLoopSetting(parameter, value);
    }

    void FocMotorCanBridge::OnQueryParameter(FocParameter parameter, const infra::Function<void(FocParameterValue)>& onDone)
    {
        onDone(HeldValue(parameter));
    }

    void FocMotorCanBridge::SetLoopSetting(FocParameter parameter, float value)
    {
        const auto completed = [this](state_machine::SettingResult result)
        {
            CompleteParameter(result);
        };

        if (GroupOf(parameter) == ParameterGroup::currentLoop)
            controlMode.SetCurrentTunings(WithSetting(controlMode.StoredCurrentTunings(), parameter, value), completed);
        else if (GroupOf(parameter) == ParameterGroup::speedLoop)
            controlMode.SetSpeedTunings(WithSetting(controlMode.StoredSpeedTunings(), parameter, value), completed);
        else
            controlMode.SetPositionTunings(WithSetting(controlMode.StoredPositionTunings(), parameter, value), completed);
    }

    // A start-up setting takes effect at the next start, so a write that fails is taken back rather than left to a later write
    void FocMotorCanBridge::SetStartupSetting(FocParameter parameter, uint32_t value)
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

    void FocMotorCanBridge::CompleteAlgorithmSelection(state_machine::SettingResult result)
    {
        auto onDone = pendingAlgorithmCallback;
        pendingAlgorithmCallback = nullptr;

        if (result == state_machine::SettingResult::ok)
            onDone(ActiveAlgorithms());
        else
            ReportSettingFailure(can::focSelectAlgorithmId, result);
    }

    void FocMotorCanBridge::CompleteParameter(state_machine::SettingResult result)
    {
        auto onDone = pendingParameterCallback;
        pendingParameterCallback = nullptr;

        if (result == state_machine::SettingResult::ok)
            onDone(HeldValue(pendingParameter));
        else
            ReportSettingFailure(can::focSetParameterId, result);
    }

    void FocMotorCanBridge::ReportSettingFailure(uint8_t commandId, state_machine::SettingResult result)
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
            default:
                server.SendCategoryError(commandId, FocMotorCategoryError::busy);
                break;
        }
    }

    FocAlgorithms FocMotorCanBridge::ActiveAlgorithms() const
    {
        return FocAlgorithms{
            state_machine::ToCanMode(controlMode.Active()),
            ToCanAlgorithm(controlMode.ActiveCurrentAlgorithm()),
            ToCanAlgorithm(controlMode.ActiveSpeedAlgorithm()),
            ToCanAlgorithm(controlMode.ActivePositionAlgorithm())
        };
    }

    FocParameterValue FocMotorCanBridge::HeldValue(FocParameter parameter) const
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

    FocParameterValue FocMotorCanBridge::HeldLoopSetting(FocParameter parameter) const
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

    FocParameterValue FocMotorCanBridge::HeldMotorParameter(FocParameter parameter) const
    {
        const auto& record = controlMode.CalibrationRecord();

        if (parameter == FocParameter::fluxLinkage && !foc::IsFinitePositive(record.fluxLinkage))
            return FocParameterValue{ controlMode.DefaultFluxLinkage().Value(), true };

        return FocParameterValue{ services::ReadMotorParameter(record, ToMotorParameter(parameter)), false };
    }

    void FocMotorCanBridge::ReportCommandOutcome(uint8_t commandId, state_machine::CommandResult result,
        const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        if (result == state_machine::CommandResult::ok)
            onDone(services::CanAckStatus::success);
        else if (result == state_machine::CommandResult::rejected)
            onDone(services::CanAckStatus::invalidState);
        else
            server.SendCategoryError(commandId, state_machine::ToCategoryError(result));
    }

    void FocMotorCanBridge::ReportSelectFailure(state_machine::SelectResult result)
    {
        if (result == state_machine::SelectResult::invalidMode)
            server.SendCommandAck(can::focSelectControlModeId, services::CanAckStatus::invalidPayload);
        else
            server.SendCategoryError(can::focSelectControlModeId, state_machine::ToCategoryError(result));
    }

    bool FocMotorCanBridge::SetpointRejected(uint8_t commandId, state_machine::ControlMode requiredMode,
        bool withinRange)
    {
        if (controlMode.Active() != requiredMode)
        {
            server.SendCategoryError(commandId, FocMotorCategoryError::modeMismatch);
            return true;
        }

        if (!withinRange)
        {
            server.SendCommandAck(commandId, services::CanAckStatus::invalidPayload);
            return true;
        }

        if (!SetpointAllowedInCurrentState())
        {
            server.SendCommandAck(commandId, services::CanAckStatus::invalidState);
            return true;
        }

        return false;
    }

    bool FocMotorCanBridge::SetpointAllowedInCurrentState() const
    {
        const auto& state = controlMode.ActiveStateMachine().CurrentState();
        return std::holds_alternative<state_machine::Ready>(state) ||
               std::holds_alternative<state_machine::Enabled>(state);
    }
}
