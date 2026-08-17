#include "core/state_machine/FocMotorCanBridge.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    int16_t ToWire(float physical, int32_t scale)
    {
        const auto scaled = static_cast<int32_t>(physical * static_cast<float>(scale));
        return static_cast<int16_t>(std::clamp<int32_t>(scaled, INT16_MIN, INT16_MAX));
    }
}

namespace state_machine
{
    FocMotorCanBridge::FocMotorCanBridge(
        services::FocMotorCategoryServer& server,
        ControlModeStateMachine& controlMode,
        const drivers::ThreePhaseInverter& inverter)
        : FocMotorCategoryServerObserver(server)
        , server(server)
        , controlMode(controlMode)
        , inverter(inverter)
    {}

    void FocMotorCanBridge::OnQueryMotorType(const infra::Function<void(services::FocMotorMode)>& onResult)
    {
        onResult(ToCanMode(controlMode.Active()));
    }

    void FocMotorCanBridge::OnStart(const infra::Function<void()>& onDone)
    {
        ReportCommandOutcome(services::focStartId, controlMode.ActiveStateMachine().CmdEnable(), onDone);
    }

    void FocMotorCanBridge::OnStop(const infra::Function<void()>& onDone)
    {
        ReportCommandOutcome(services::focStopId, controlMode.ActiveStateMachine().CmdDisable(), onDone);
    }

    // Raw PID gains are no longer part of the FOC contract; tuning is bandwidth-based.
    // can-lite has no bandwidth message yet, so these legacy frames are rejected rather than
    // reinterpreted, which would apply a gain value as a bandwidth on a live motor.
    void FocMotorCanBridge::OnSetPidCurrent(const services::FocPidGains&, const infra::Function<void()>&)
    {
        server.SendCategoryError(services::focSetPidCurrentId, services::FocMotorCategoryError::applicationError);
    }

    void FocMotorCanBridge::OnSetPidSpeed(const services::FocPidGains&, const infra::Function<void()>&)
    {
        server.SendCategoryError(services::focSetPidSpeedId, services::FocMotorCategoryError::applicationError);
    }

    void FocMotorCanBridge::OnSetPidPosition(const services::FocPidGains&, const infra::Function<void()>&)
    {
        server.SendCategoryError(services::focSetPidPositionId, services::FocMotorCategoryError::applicationError);
    }

    void FocMotorCanBridge::OnIdentifyElectrical(const infra::Function<void(services::FocElectricalParams)>& onResult)
    {
        if (pendingIdentifyCallback != nullptr)
        {
            server.SendCategoryError(services::focIdentifyElectricalId, services::FocMotorCategoryError::busy);
            return;
        }

        pendingIdentifyCallback = onResult;
        controlMode.ActiveStateMachine().CmdCalibrate([this](state_machine::CommandResult result)
            {
                if (result == state_machine::CommandResult::ok)
                    DeliverIdentifiedElectricalParams();
                else
                {
                    pendingIdentifyCallback = nullptr;
                    server.SendCategoryError(services::focIdentifyElectricalId, ToCategoryError(result));
                }
            });
    }

    void FocMotorCanBridge::OnIdentifyMechanical(const infra::Function<void(services::FocMechanicalParams)>&)
    {
        server.SendCommandAck(services::focIdentifyMechanicalId, services::CanAckStatus::notImplemented);
    }

    void FocMotorCanBridge::OnRequestTelemetry(const infra::Function<void(services::FocTelemetryElectrical, services::FocTelemetryStatus)>&)
    {
        server.SendCommandAck(services::focRequestTelemetryId, services::CanAckStatus::notImplemented);
    }

    void FocMotorCanBridge::OnSetEncoderResolution(uint16_t, const infra::Function<void()>&)
    {
        server.SendCommandAck(services::focSetEncoderResolutionId, services::CanAckStatus::notImplemented);
    }

    void FocMotorCanBridge::OnSelectControlMode(services::FocMotorMode requestedMode, const infra::Function<void(services::FocMotorMode)>& onActivated)
    {
        if (pendingSelectCallback != nullptr)
        {
            server.SendCategoryError(services::focSelectControlModeId, services::FocMotorCategoryError::busy);
            return;
        }

        const auto mode = FromCanMode(requestedMode);
        if (!mode.has_value())
        {
            ReportSelectFailure(SelectResult::invalidMode);
            return;
        }

        pendingSelectCallback = onActivated;
        controlMode.Select(*mode, [this](SelectResult result)
            {
                auto callback = pendingSelectCallback;
                pendingSelectCallback = nullptr;
                if (result == SelectResult::ok)
                    callback(ToCanMode(controlMode.Active()));
                else
                    ReportSelectFailure(result);
            });
    }

    void FocMotorCanBridge::OnSetTorqueSetpoint(int16_t value, const infra::Function<void()>& onDone)
    {
        const foc::Ampere iq{ static_cast<float>(value) / static_cast<float>(services::focCurrentScale) };
        if (SetpointRejected(services::focSetTorqueSetpointId, ControlMode::torque,
                std::fabs(iq.Value()) <= inverter.MaxCurrentSupported().Value()))
            return;

        if (controlMode.TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, iq }))
            onDone();
        else
            server.SendCategoryError(services::focSetTorqueSetpointId, services::FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnSetSpeedSetpoint(int16_t value, const infra::Function<void()>& onDone)
    {
        const foc::RadiansPerSecond setpoint{ static_cast<float>(value) / static_cast<float>(services::focSpeedScale) };
        if (SetpointRejected(services::focSetSpeedSetpointId, ControlMode::speed,
                std::fabs(setpoint.Value()) <= maxSpeedSetpoint))
            return;

        if (controlMode.TrySetSpeed(setpoint))
            onDone();
        else
            server.SendCategoryError(services::focSetSpeedSetpointId, services::FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnSetPositionSetpoint(int16_t value, const infra::Function<void()>& onDone)
    {
        const foc::Radians setpoint{ static_cast<float>(value) / static_cast<float>(services::focPositionScale) };
        if (SetpointRejected(services::focSetPositionSetpointId, ControlMode::position,
                std::fabs(setpoint.Value()) <= maxPositionSetpoint))
            return;

        if (controlMode.TrySetPosition(setpoint))
            onDone();
        else
            server.SendCategoryError(services::focSetPositionSetpointId, services::FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnClearFault(const infra::Function<void()>& onDone)
    {
        ReportCommandOutcome(services::focClearFaultId, controlMode.ActiveStateMachine().CmdClearFault(), onDone);
    }

    void FocMotorCanBridge::OnEmergencyStop(const infra::Function<void()>& onDone)
    {
        ReportCommandOutcome(services::focEmergencyStopId, controlMode.ActiveStateMachine().CmdEmergencyStop(), onDone);
    }

    void FocMotorCanBridge::OnConfigureTelemetryRate(uint8_t, const infra::Function<void()>&)
    {
        server.SendCommandAck(services::focConfigureTelemetryRateId, services::CanAckStatus::notImplemented);
    }

    void FocMotorCanBridge::ReportCommandOutcome(uint8_t commandId, CommandResult result, const infra::Function<void()>& onDone)
    {
        if (result == CommandResult::ok)
            onDone();
        else if (result == CommandResult::rejected)
            server.SendCommandAck(commandId, services::CanAckStatus::invalidState);
        else
            server.SendCategoryError(commandId, ToCategoryError(result));
    }

    void FocMotorCanBridge::ReportSelectFailure(SelectResult result)
    {
        if (result == SelectResult::invalidMode)
            server.SendCommandAck(services::focSelectControlModeId, services::CanAckStatus::invalidPayload);
        else
            server.SendCategoryError(services::focSelectControlModeId, ToCategoryError(result));
    }

    bool FocMotorCanBridge::SetpointRejected(uint8_t commandId, ControlMode requiredMode, bool withinRange)
    {
        if (!withinRange)
        {
            server.SendCommandAck(commandId, services::CanAckStatus::invalidPayload);
            return true;
        }

        if (controlMode.Active() != requiredMode)
        {
            server.SendCategoryError(commandId, services::FocMotorCategoryError::modeMismatch);
            return true;
        }

        if (!SetpointAllowedInCurrentState())
        {
            server.SendCommandAck(commandId, services::CanAckStatus::invalidState);
            return true;
        }

        return false;
    }

    // A stored setpoint is re-applied by the cascade on the next enable, so only the states from
    // which the motor can actually run may accept one.
    bool FocMotorCanBridge::SetpointAllowedInCurrentState() const
    {
        const auto& state = controlMode.ActiveStateMachine().CurrentState();
        return std::holds_alternative<Ready>(state) || std::holds_alternative<Enabled>(state);
    }

    void FocMotorCanBridge::DeliverIdentifiedElectricalParams()
    {
        auto callback = pendingIdentifyCallback;
        pendingIdentifyCallback = nullptr;

        const auto* ready = std::get_if<Ready>(&controlMode.ActiveStateMachine().CurrentState());
        if (ready == nullptr)
        {
            server.SendCategoryError(services::focIdentifyElectricalId, services::FocMotorCategoryError::calibrationFailed);
            return;
        }

        // lD is held in millihenry, so the wire value carries microhenry resolution.
        callback(services::FocElectricalParams{
            ToWire(ready->loadedData.rPhase, services::focResistanceScale),
            ToWire(ready->loadedData.lD, services::focInductanceScale) });
    }
}
