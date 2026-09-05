#pragma once

#include "core/can/FocMotorCategoryServer.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "infra/util/Function.hpp"
#include "services/tracer/Tracer.hpp"
#include <optional>

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
            services::ElectricalParametersIdentification& electricalIdent,
            services::MechanicalParametersIdentification* mechIdent,
            foc::NewtonMeter mechTorqueConstant,
            services::NonVolatileMemory& nvm,
            services::ConfigData configData,
            services::Tracer& tracer);

        static constexpr float maxPositionSetpoint{ foc::CommandLimits::maxPositionSetpoint };
        static constexpr float maxSpeedSetpoint{ foc::CommandLimits::maxSpeedSetpoint };

        void BroadcastFault(state_machine::FaultCode code);

        void OnStart(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnStop(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnClearFault(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnEmergencyStop(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnSelectControlMode(FocMotorMode mode, const infra::Function<void(FocMotorMode)>& onActivated) override;
        void OnSetTorqueSetpoint(foc::Ampere value, const infra::Function<void()>& onDone) override;
        void OnSetSpeedSetpoint(foc::RadiansPerSecond value, const infra::Function<void()>& onDone) override;
        void OnSetPositionSetpoint(foc::Radians value, const infra::Function<void()>& onDone) override;

        void OnSetPidCurrent(float bandwidth, const infra::Function<void()>& onDone) override;
        void OnSetPidSpeed(float bandwidth, const infra::Function<void()>& onDone) override;
        void OnSetPidPosition(float bandwidth, const infra::Function<void()>& onDone) override;
        void OnIdentifyElectrical(const infra::Function<void()>& onDone) override;
        void OnIdentifyMechanical(const infra::Function<void()>& onDone) override;
        void OnRequestTelemetry(const infra::Function<void()>& onDone) override;
        void OnSetEncoderResolution(uint32_t resolution, const infra::Function<void()>& onDone) override;
        void OnConfigureTelemetryRate(uint32_t rateHz, const infra::Function<void()>& onDone) override;

    private:
        static FocFaultCode ToCanFaultCode(state_machine::FaultCode code);
        static FocMotorState ToCanMotorState(const state_machine::State& state);

        void ReportCommandOutcome(uint8_t commandId, state_machine::CommandResult result,
            const infra::Function<void(services::CanAckStatus)>& onDone);
        void ReportSelectFailure(state_machine::SelectResult result);
        bool SetpointRejected(uint8_t commandId, state_machine::ControlMode requiredMode, bool withinRange);
        bool SetpointAllowedInCurrentState() const;

        FocMotorCategoryServer& server;
        state_machine::ControlModeStateMachine& controlMode;
        const drivers::ThreePhaseInverter& inverter;
        services::ElectricalParametersIdentification& electricalIdent;
        services::MechanicalParametersIdentification* mechIdent;
        foc::NewtonMeter mechTorqueConstant;
        services::NonVolatileMemory& nvm;
        services::ConfigData configData;

        infra::Function<void(FocMotorMode)> pendingSelectCallback;
        infra::Function<void()> pendingElectricalIdentDoneCallback;
        infra::Function<void()> pendingMechIdentDoneCallback;
        infra::Function<void()> pendingNvmDoneCallback;
        std::optional<foc::Ohm> pendingElectricalR;
        std::optional<foc::MilliHenry> pendingElectricalL;
    };
}
