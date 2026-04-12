#pragma once

#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "core/state_machine/FocStateMachine.hpp"

namespace integration
{
    class FocMotorStateMachineBridge
        : public services::FocMotorCategoryServerObserver
    {
    public:
        FocMotorStateMachineBridge(services::FocMotorCategoryServer& server,
            state_machine::FocStateMachineBase& stateMachine);

        void OnQueryMotorType() override;
        void OnStart() override;
        void OnStop() override;
        void OnSetPidCurrent(const services::FocPidGains&) override;
        void OnSetPidSpeed(const services::FocPidGains&) override;
        void OnSetPidPosition(const services::FocPidGains&) override;
        void OnIdentifyElectrical() override;
        void OnIdentifyMechanical() override;
        void OnRequestTelemetry() override;
        void OnSetEncoderResolution(uint16_t) override;
        void OnSetTarget(const services::FocSetpoint&) override;
        void OnClearFault() override;
        void OnEmergencyStop() override;
        void OnConfigureTelemetryRate(uint8_t) override;

    private:
        state_machine::FocStateMachineBase& stateMachine;
    };
}
