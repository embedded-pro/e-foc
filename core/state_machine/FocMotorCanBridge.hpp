#pragma once

#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "infra/util/Function.hpp"

namespace state_machine
{
    class FocMotorCanBridge
        : public services::FocMotorCategoryServerObserver
    {
    public:
        FocMotorCanBridge(services::FocMotorCategoryServer& server, ControlModeStateMachine& controlMode, const drivers::ThreePhaseInverter& inverter);

        void OnQueryMotorType(const infra::Function<void(services::FocMotorMode)>&) override;
        void OnStart(const infra::Function<void()>& onDone) override;
        void OnStop(const infra::Function<void()>& onDone) override;
        void OnSetPidCurrent(const services::FocPidGains&, const infra::Function<void()>&) override;
        void OnSetPidSpeed(const services::FocPidGains&, const infra::Function<void()>&) override;
        void OnSetPidPosition(const services::FocPidGains&, const infra::Function<void()>&) override;
        void OnIdentifyElectrical(const infra::Function<void(services::FocElectricalParams)>& onResult) override;
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

        // The position law folds its error into [-pi, pi], so a request beyond one mechanical
        // revolution aliases onto an angle inside it and can only be a scaling mistake.
        static constexpr float maxPositionSetpoint{ 6.2831853f };

        // The position cascade never commands more than +-1000 rad/s (documentation/design/foc-position.md),
        // so a speed commanded directly over the bus is bounded the same way.
        static constexpr float maxSpeedSetpoint{ 1000.0f };

    private:
        void ReportCommandOutcome(uint8_t commandId, CommandResult result, const infra::Function<void()>& onDone);
        void ReportSelectFailure(SelectResult result);
        bool SetpointRejected(uint8_t commandId, ControlMode requiredMode, bool withinRange);
        bool SetpointAllowedInCurrentState() const;
        void DeliverIdentifiedElectricalParams();

        services::FocMotorCategoryServer& server;
        ControlModeStateMachine& controlMode;
        const drivers::ThreePhaseInverter& inverter;
        infra::Function<void(services::FocMotorMode)> pendingSelectCallback;
        infra::Function<void(services::FocElectricalParams)> pendingIdentifyCallback;
    };
}
