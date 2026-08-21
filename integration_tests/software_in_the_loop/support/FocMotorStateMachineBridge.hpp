#pragma once

#include "core/can/FocMotorCategoryServer.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/util/Function.hpp"

namespace integration
{
    class FocMotorStateMachineBridge
        : public can::FocMotorCategoryServerObserver
    {
    public:
        FocMotorStateMachineBridge(can::FocMotorCategoryServer& server,
            state_machine::FocStateMachineBase& stateMachine);

        void OnStart(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnStop(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnClearFault(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnEmergencyStop(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnSelectControlMode(can::FocMotorMode, const infra::Function<void(can::FocMotorMode)>&) override;
        void OnSetTorqueSetpoint(foc::Ampere, const infra::Function<void()>&) override;
        void OnSetSpeedSetpoint(foc::RadiansPerSecond, const infra::Function<void()>&) override;
        void OnSetPositionSetpoint(foc::Radians, const infra::Function<void()>&) override;

        void OnSetPidCurrent(const infra::Function<void()>&) override;
        void OnSetPidSpeed(const infra::Function<void()>&) override;
        void OnSetPidPosition(const infra::Function<void()>&) override;
        void OnIdentifyElectrical(const infra::Function<void()>&) override;
        void OnIdentifyMechanical(const infra::Function<void()>&) override;
        void OnRequestTelemetry(const infra::Function<void()>&) override;
        void OnSetEncoderResolution(const infra::Function<void()>&) override;
        void OnConfigureTelemetryRate(const infra::Function<void()>&) override;

    private:
        state_machine::FocStateMachineBase& stateMachine;
    };
}
