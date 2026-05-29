#include "core/state_machine/FocMotorCanBridge.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace state_machine
{
    FocMotorCanBridge::FocMotorCanBridge(
        services::FocMotorCategoryServer& server,
        ControlModeStateMachine& controlMode)
        : FocMotorCategoryServerObserver(server)
        , server(server)
        , controlMode(controlMode)
    {}

    void FocMotorCanBridge::OnQueryMotorType(const infra::Function<void(services::FocMotorMode)>& onResult)
    {
        onResult(ToCanMode(controlMode.Active()));
    }

    void FocMotorCanBridge::OnStart(const infra::Function<void()>& onDone)
    {
        controlMode.ActiveStateMachine().CmdEnable();
        onDone();
    }

    void FocMotorCanBridge::OnStop(const infra::Function<void()>& onDone)
    {
        controlMode.ActiveStateMachine().CmdDisable();
        onDone();
    }

    void FocMotorCanBridge::OnSetPidCurrent(const services::FocPidGains& gains, const infra::Function<void()>& onDone)
    {
        if (controlMode.TrySetCurrentPidGains(gains))
            onDone();
        else
            server.SendCategoryError(services::focSetPidCurrentId, services::FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnSetPidSpeed(const services::FocPidGains& gains, const infra::Function<void()>& onDone)
    {
        if (controlMode.TrySetSpeedPidGains(gains))
            onDone();
        else
            server.SendCategoryError(services::focSetPidSpeedId, services::FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnSetPidPosition(const services::FocPidGains& gains, const infra::Function<void()>& onDone)
    {
        if (controlMode.TrySetPositionPidGains(gains))
            onDone();
        else
            server.SendCategoryError(services::focSetPidPositionId, services::FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnIdentifyElectrical(const infra::Function<void(services::FocElectricalParams)>&)
    {
        controlMode.ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
        server.SendCommandAck(services::focIdentifyElectricalId, services::CanAckStatus::success);
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
        really_assert(pendingSelectCallback == nullptr);
        pendingSelectCallback = onActivated;
        controlMode.Select(FromCanMode(requestedMode), [this](SelectResult result)
            {
                auto callback = pendingSelectCallback;
                pendingSelectCallback = nullptr;
                if (result == SelectResult::ok)
                    callback(ToCanMode(controlMode.Active()));
                else if (result == SelectResult::invalidMode)
                    server.SendCommandAck(services::focSelectControlModeId, services::CanAckStatus::invalidPayload);
                else
                    server.SendCategoryError(services::focSelectControlModeId, ToCategoryError(result));
            });
    }

    void FocMotorCanBridge::OnSetTorqueSetpoint(int16_t value, const infra::Function<void()>& onDone)
    {
        const foc::IdAndIqPoint setpoint{ foc::Ampere{ 0.0f }, foc::Ampere{ static_cast<float>(value) / static_cast<float>(services::focCurrentScale) } };
        if (controlMode.TrySetTorque(setpoint))
            onDone();
        else
            server.SendCategoryError(services::focSetTorqueSetpointId, services::FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnSetSpeedSetpoint(int16_t value, const infra::Function<void()>& onDone)
    {
        const foc::RadiansPerSecond setpoint{ static_cast<float>(value) / static_cast<float>(services::focSpeedScale) };
        if (controlMode.TrySetSpeed(setpoint))
            onDone();
        else
            server.SendCategoryError(services::focSetSpeedSetpointId, services::FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnSetPositionSetpoint(int16_t value, const infra::Function<void()>& onDone)
    {
        const foc::Radians setpoint{ static_cast<float>(value) / static_cast<float>(services::focPositionScale) };
        if (controlMode.TrySetPosition(setpoint))
            onDone();
        else
            server.SendCategoryError(services::focSetPositionSetpointId, services::FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnClearFault(const infra::Function<void()>& onDone)
    {
        controlMode.ActiveStateMachine().CmdClearFault();
        onDone();
    }

    void FocMotorCanBridge::OnEmergencyStop(const infra::Function<void()>& onDone)
    {
        controlMode.ActiveStateMachine().CmdEmergencyStop();
        onDone();
    }

    void FocMotorCanBridge::OnConfigureTelemetryRate(uint8_t, const infra::Function<void()>&)
    {
        server.SendCommandAck(services::focConfigureTelemetryRateId, services::CanAckStatus::notImplemented);
    }
}
