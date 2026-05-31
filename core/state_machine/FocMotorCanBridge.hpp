#pragma once

#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "infra/util/Function.hpp"

namespace state_machine
{
    class FocMotorCanBridge
        : public services::FocMotorCategoryServerObserver
    {
    public:
        FocMotorCanBridge(services::FocMotorCategoryServer& server, ControlModeStateMachine& controlMode);

        void OnFault(FaultCode code);

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
        void OnSelectControlMode(services::FocMotorMode requestedMode, const infra::Function<void(services::FocMotorMode)>& onActivated) override;
        void OnSetTorqueSetpoint(int16_t value, const infra::Function<void()>& onDone) override;
        void OnSetSpeedSetpoint(int16_t value, const infra::Function<void()>& onDone) override;
        void OnSetPositionSetpoint(int16_t value, const infra::Function<void()>& onDone) override;
        void OnClearFault(const infra::Function<void()>& onDone) override;
        void OnEmergencyStop(const infra::Function<void()>& onDone) override;
        void OnConfigureTelemetryRate(uint8_t, const infra::Function<void()>&) override;

    private:
        static services::FocFaultCode ToCanFaultCode(FaultCode code);

        services::FocMotorCategoryServer& server;
        ControlModeStateMachine& controlMode;
        infra::Function<void(services::FocMotorMode)> pendingSelectCallback;
    };
}
