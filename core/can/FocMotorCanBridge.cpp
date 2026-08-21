#include "core/can/FocMotorCanBridge.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include <cmath>

namespace can
{
    FocMotorCanBridge::FocMotorCanBridge(
        FocMotorCategoryServer& server,
        state_machine::ControlModeStateMachine& controlMode,
        const drivers::ThreePhaseInverter& inverter,
        services::Tracer& tracer)
        : FocMotorCategoryServerObserver(server)
        , server(server)
        , controlMode(controlMode)
        , inverter(inverter)
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
            case state_machine::FaultCode::overcurrent:     return FocFaultCode::overCurrent;
            case state_machine::FaultCode::overvoltage:     return FocFaultCode::overVoltage;
            case state_machine::FaultCode::overtemperature: return FocFaultCode::overTemperature;
            case state_machine::FaultCode::encoderLoss:     return FocFaultCode::sensorFault;
            default:                                         return FocFaultCode::none;
        }
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

    void FocMotorCanBridge::OnSetPidCurrent(const infra::Function<void()>&)
    {
        server.SendCategoryError(can::focSetPidCurrentId, FocMotorCategoryError::applicationError);
    }

    void FocMotorCanBridge::OnSetPidSpeed(const infra::Function<void()>&)
    {
        server.SendCategoryError(can::focSetPidSpeedId, FocMotorCategoryError::applicationError);
    }

    void FocMotorCanBridge::OnSetPidPosition(const infra::Function<void()>&)
    {
        server.SendCategoryError(can::focSetPidPositionId, FocMotorCategoryError::applicationError);
    }

    void FocMotorCanBridge::OnIdentifyElectrical(const infra::Function<void()>&)
    {
        server.SendCategoryError(can::focIdentifyElectricalId, FocMotorCategoryError::applicationError);
    }

    void FocMotorCanBridge::OnIdentifyMechanical(const infra::Function<void()>&)
    {
        server.SendCategoryError(can::focIdentifyMechanicalId, FocMotorCategoryError::applicationError);
    }

    void FocMotorCanBridge::OnRequestTelemetry(const infra::Function<void()>&)
    {
        server.SendCategoryError(can::focRequestTelemetryId, FocMotorCategoryError::applicationError);
    }

    void FocMotorCanBridge::OnSetEncoderResolution(const infra::Function<void()>&)
    {
        server.SendCategoryError(can::focSetEncoderResolutionId, FocMotorCategoryError::applicationError);
    }

    void FocMotorCanBridge::OnConfigureTelemetryRate(const infra::Function<void()>&)
    {
        server.SendCategoryError(can::focConfigureTelemetryRateId, FocMotorCategoryError::applicationError);
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
