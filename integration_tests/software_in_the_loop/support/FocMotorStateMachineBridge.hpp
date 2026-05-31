#pragma once

#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/util/Function.hpp"

namespace integration
{
    class FocMotorStateMachineBridge
        : public services::FocMotorCategoryServerObserver
    {
    public:
        FocMotorStateMachineBridge(services::FocMotorCategoryServer& server,
            state_machine::FocStateMachineBase& stateMachine);

        void OnQueryMotorType(const infra::Function<void(services::FocMotorMode)>&) override;
        void OnStart(const infra::Function<void()>& onDone) override;
        void OnStop(const infra::Function<void()>& onDone) override;
        void OnSetPidCurrent(const services::FocPidGains&, const infra::Function<void()>&) override;
        void OnSetPidSpeed(const services::FocPidGains&, const infra::Function<void()>&) override;
        void OnSetPidPosition(const services::FocPidGains&, const infra::Function<void()>&) override;
        void OnIdentifyElectrical(const infra::Function<void(services::FocElectricalParams)>&) override;
        void OnIdentifyMechanical(const infra::Function<void(services::FocMechanicalParams)>&) override;
        void OnRequestTelemetry(const infra::Function<void(services::FocTelemetryElectrical, services::FocTelemetryStatus)>&) override;
        void OnSetEncoderResolution(uint16_t, const infra::Function<void()>&) override;
        void OnSelectControlMode(services::FocMotorMode, const infra::Function<void(services::FocMotorMode)>&) override;
        void OnSetTorqueSetpoint(int16_t, const infra::Function<void()>&) override;
        void OnSetSpeedSetpoint(int16_t, const infra::Function<void()>&) override;
        void OnSetPositionSetpoint(int16_t, const infra::Function<void()>&) override;
        void OnClearFault(const infra::Function<void()>& onDone) override;
        void OnEmergencyStop(const infra::Function<void()>& onDone) override;
        void OnConfigureTelemetryRate(uint8_t, const infra::Function<void()>&) override;

    private:
        state_machine::FocStateMachineBase& stateMachine;
    };
}
