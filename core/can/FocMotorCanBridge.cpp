#include "core/can/FocMotorCanBridge.hpp"
#include "core/can/FocMotorCommandOutcome.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include <cmath>
#include <limits>
#include <variant>

namespace can
{
    FocMotorCanBridge::FocMotorCanBridge(
        FocMotorCategoryServer& server,
        state_machine::ControlModeStateMachine& controlMode,
        const Platform& platform,
        services::ElectricalParametersIdentification& electricalIdent,
        services::Tracer& tracer,
        std::optional<std::reference_wrapper<services::PhaseCurrentOffsets>> currentOffsets,
        std::optional<std::reference_wrapper<services::RotorStandstill>> standstill)
        : FocMotorCategoryServerObserver(server)
        , server(server)
        , controlMode(controlMode)
        , inverter(platform.inverter)
        , encoder(platform.encoder)
        , commissioning(server, controlMode, electricalIdent, currentOffsets, standstill)
        , settings(server, controlMode, platform.diagnostics, platform.resetCause)
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
            case state_machine::FaultCode::stall:
                return FocFaultCode::stall;
            case state_machine::FaultCode::phaseMissing:
                return FocFaultCode::phaseMissing;
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
        ReportCommandOutcome(server, can::focStartId, controlMode.ActiveStateMachine().CmdEnable(), onDone);
    }

    void FocMotorCanBridge::OnStop(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        ReportCommandOutcome(server, can::focStopId, controlMode.ActiveStateMachine().CmdDisable(), onDone);
    }

    void FocMotorCanBridge::OnClearFault(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        ReportCommandOutcome(server, can::focClearFaultId, controlMode.ActiveStateMachine().CmdClearFault(), onDone);
    }

    void FocMotorCanBridge::OnEmergencyStop(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        ReportCommandOutcome(server, can::focEmergencyStopId, controlMode.ActiveStateMachine().CmdEmergencyStop(), onDone);
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
        settings.SetLegacyBandwidth(can::focSetPidCurrentId, FocParameter::currentBandwidth, bandwidth, onDone);
    }

    void FocMotorCanBridge::OnSetPidSpeed(float bandwidth, const infra::Function<void()>& onDone)
    {
        settings.SetLegacyBandwidth(can::focSetPidSpeedId, FocParameter::speedBandwidth, bandwidth, onDone);
    }

    void FocMotorCanBridge::OnSetPidPosition(float bandwidth, const infra::Function<void()>& onDone)
    {
        settings.SetLegacyBandwidth(can::focSetPidPositionId, FocParameter::positionBandwidth, bandwidth, onDone);
    }

    void FocMotorCanBridge::OnAlign(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        commissioning.Align(onDone);
    }

    void FocMotorCanBridge::OnIdentifyElectrical(const infra::Function<void()>& onDone)
    {
        commissioning.IdentifyElectrical(onDone);
    }

    void FocMotorCanBridge::OnCalibrate(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        commissioning.Calibrate(onDone);
    }

    void FocMotorCanBridge::OnClearCalibration(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        commissioning.ClearCalibration(onDone);
    }

    void FocMotorCanBridge::OnClearDiagnostics(const infra::Function<void()>& onDone)
    {
        settings.ClearDiagnostics();
        onDone();
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
        settings.SetEncoderResolution(resolution, onDone);
    }

    void FocMotorCanBridge::OnConfigureTelemetryRate(uint32_t rateHz, const infra::Function<void()>& onDone)
    {
        settings.ConfigureTelemetryRate(rateHz, onDone);
    }

    void FocMotorCanBridge::OnSelectAlgorithm(FocControlLoop loop, uint8_t algorithm, const infra::Function<void(const FocAlgorithms&)>& onDone)
    {
        settings.SelectAlgorithm(loop, algorithm, onDone);
    }

    void FocMotorCanBridge::OnQueryAlgorithms(const infra::Function<void(const FocAlgorithms&)>& onDone)
    {
        onDone(settings.ActiveAlgorithms());
    }

    void FocMotorCanBridge::OnSetParameter(FocParameter parameter, float value, const infra::Function<void(FocParameterValue)>& onDone)
    {
        settings.SetParameter(parameter, value, onDone);
    }

    void FocMotorCanBridge::OnQueryParameter(FocParameter parameter, const infra::Function<void(FocParameterValue)>& onDone)
    {
        onDone(settings.HeldValue(parameter));
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
