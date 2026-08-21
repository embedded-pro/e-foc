#include "integration_tests/software_in_the_loop/support/FocMotorStateMachineBridge.hpp"

namespace integration
{
    FocMotorStateMachineBridge::FocMotorStateMachineBridge(can::FocMotorCategoryServer& server,
        state_machine::FocStateMachineBase& stateMachine)
        : FocMotorCategoryServerObserver(server)
        , stateMachine(stateMachine)
    {}

    void FocMotorStateMachineBridge::OnStart(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        stateMachine.CmdEnable();
        onDone(services::CanAckStatus::success);
    }

    void FocMotorStateMachineBridge::OnStop(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        stateMachine.CmdDisable();
        onDone(services::CanAckStatus::success);
    }

    void FocMotorStateMachineBridge::OnClearFault(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        stateMachine.CmdClearFault();
        onDone(services::CanAckStatus::success);
    }

    void FocMotorStateMachineBridge::OnEmergencyStop(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        stateMachine.CmdEmergencyStop();
        onDone(services::CanAckStatus::success);
    }

    void FocMotorStateMachineBridge::OnSelectControlMode(can::FocMotorMode, const infra::Function<void(can::FocMotorMode)>&)
    {}

    void FocMotorStateMachineBridge::OnSetTorqueSetpoint(foc::Ampere, const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnSetSpeedSetpoint(foc::RadiansPerSecond, const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnSetPositionSetpoint(foc::Radians, const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnSetPidCurrent(const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnSetPidSpeed(const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnSetPidPosition(const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnIdentifyElectrical(const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnIdentifyMechanical(const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnRequestTelemetry(const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnSetEncoderResolution(const infra::Function<void()>&)
    {}

    void FocMotorStateMachineBridge::OnConfigureTelemetryRate(const infra::Function<void()>&)
    {}
}
