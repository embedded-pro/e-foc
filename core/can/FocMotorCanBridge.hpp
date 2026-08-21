#pragma once

#include "core/can/FocMotorCategoryServer.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "infra/util/Function.hpp"
#include "services/tracer/Tracer.hpp"

namespace can
{
    class FocMotorCanBridge
        : public FocMotorCategoryServerObserver
    {
    public:
        FocMotorCanBridge(
            FocMotorCategoryServer& server,
            state_machine::ControlModeStateMachine& controlMode,
            const drivers::ThreePhaseInverter& inverter,
            services::Tracer& tracer);

        static constexpr float maxPositionSetpoint{ 6.2831853f };
        static constexpr float maxSpeedSetpoint{ 1000.0f };

        void OnStart(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnStop(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnClearFault(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnEmergencyStop(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnSelectControlMode(FocMotorMode mode, const infra::Function<void(FocMotorMode)>& onActivated) override;
        void OnSetTorqueSetpoint(foc::Ampere value, const infra::Function<void()>& onDone) override;
        void OnSetSpeedSetpoint(foc::RadiansPerSecond value, const infra::Function<void()>& onDone) override;
        void OnSetPositionSetpoint(foc::Radians value, const infra::Function<void()>& onDone) override;

        void OnSetPidCurrent(const infra::Function<void()>& onDone) override;
        void OnSetPidSpeed(const infra::Function<void()>& onDone) override;
        void OnSetPidPosition(const infra::Function<void()>& onDone) override;
        void OnIdentifyElectrical(const infra::Function<void()>& onDone) override;
        void OnIdentifyMechanical(const infra::Function<void()>& onDone) override;
        void OnRequestTelemetry(const infra::Function<void()>& onDone) override;
        void OnSetEncoderResolution(const infra::Function<void()>& onDone) override;
        void OnConfigureTelemetryRate(const infra::Function<void()>& onDone) override;

    private:
        void ReportCommandOutcome(uint8_t commandId, state_machine::CommandResult result,
            const infra::Function<void(services::CanAckStatus)>& onDone);
        void ReportSelectFailure(state_machine::SelectResult result);
        bool SetpointRejected(uint8_t commandId, state_machine::ControlMode requiredMode, bool withinRange);
        bool SetpointAllowedInCurrentState() const;

        FocMotorCategoryServer& server;
        state_machine::ControlModeStateMachine& controlMode;
        const drivers::ThreePhaseInverter& inverter;
        infra::Function<void(FocMotorMode)> pendingSelectCallback;
    };
}
