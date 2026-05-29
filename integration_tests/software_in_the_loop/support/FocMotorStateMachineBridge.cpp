#include "integration_tests/software_in_the_loop/support/FocMotorStateMachineBridge.hpp"

namespace integration
{
    FocMotorStateMachineBridge::FocMotorStateMachineBridge(services::FocMotorCategoryServer& server,
        state_machine::FocStateMachineBase& stateMachine)
        : FocMotorCategoryServerObserver(server)
        , stateMachine(stateMachine)
    {}

    void FocMotorStateMachineBridge::OnQueryMotorType(const infra::Function<void(services::FocMotorMode)>&)
    {}

    void FocMotorStateMachineBridge::OnStart(const infra::Function<void()>& onDone)
    {
        stateMachine.CmdEnable();
        onDone();
    }

    void FocMotorStateMachineBridge::OnStop(const infra::Function<void()>& onDone)
    {
        stateMachine.CmdDisable();
        onDone();
    }

    void FocMotorStateMachineBridge::OnSetPidCurrent(const services::FocPidGains&, const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnSetPidSpeed(const services::FocPidGains&, const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnSetPidPosition(const services::FocPidGains&, const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnIdentifyElectrical(const infra::Function<void(services::FocElectricalParams)>&)
    {}

    void FocMotorStateMachineBridge::OnIdentifyMechanical(const infra::Function<void(services::FocMechanicalParams)>&)
    {}

    void FocMotorStateMachineBridge::OnRequestTelemetry(const infra::Function<void(services::FocTelemetryElectrical, services::FocTelemetryStatus)>&)
    {}

    void FocMotorStateMachineBridge::OnSetEncoderResolution(uint16_t, const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnSelectControlMode(services::FocMotorMode, const infra::Function<void(services::FocMotorMode)>&)
    {}

    void FocMotorStateMachineBridge::OnSetTorqueSetpoint(int16_t, const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnSetSpeedSetpoint(int16_t, const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnSetPositionSetpoint(int16_t, const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnClearFault(const infra::Function<void()>& onDone)
    {
        stateMachine.CmdClearFault();
        onDone();
    }

    void FocMotorStateMachineBridge::OnEmergencyStop(const infra::Function<void()>& onDone)
    {
        stateMachine.CmdDisable();
        onDone();
    }

    void FocMotorStateMachineBridge::OnConfigureTelemetryRate(uint8_t, const infra::Function<void()>&)
    {}
}
