#include "integration_tests/support/FocMotorStateMachineBridge.hpp"

namespace integration
{
    FocMotorStateMachineBridge::FocMotorStateMachineBridge(services::FocMotorCategoryServer& server,
        state_machine::FocStateMachineBase& stateMachine)
        : FocMotorCategoryServerObserver(server)
        , stateMachine(stateMachine)
    {}

    void FocMotorStateMachineBridge::OnQueryMotorType()
    {}

    void FocMotorStateMachineBridge::OnStart()
    {
        stateMachine.CmdEnable();
    }

    void FocMotorStateMachineBridge::OnStop()
    {
        stateMachine.CmdDisable();
    }

    void FocMotorStateMachineBridge::OnSetPidCurrent(const services::FocPidGains&)
    {}

    void FocMotorStateMachineBridge::OnSetPidSpeed(const services::FocPidGains&)
    {}

    void FocMotorStateMachineBridge::OnSetPidPosition(const services::FocPidGains&)
    {}

    void FocMotorStateMachineBridge::OnIdentifyElectrical()
    {}

    void FocMotorStateMachineBridge::OnIdentifyMechanical()
    {}

    void FocMotorStateMachineBridge::OnRequestTelemetry()
    {}

    void FocMotorStateMachineBridge::OnSetEncoderResolution(uint16_t)
    {}

    void FocMotorStateMachineBridge::OnSetTarget(const services::FocSetpoint&)
    {}

    void FocMotorStateMachineBridge::OnClearFault()
    {
        stateMachine.CmdClearFault();
    }

    void FocMotorStateMachineBridge::OnEmergencyStop()
    {
        stateMachine.CmdDisable();
    }

    void FocMotorStateMachineBridge::OnConfigureTelemetryRate(uint8_t)
    {}
}
