#pragma once

#include "core/can/FocMotorCategoryServer.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
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
            drivers::Encoder& encoder,
            services::ElectricalParametersIdentification& electricalIdent,
            services::MechanicalParametersIdentification* mechIdent,
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
        void OnAlign(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnIdentifyElectrical(const infra::Function<void()>& onDone) override;
        void OnIdentifyMechanical(const infra::Function<void()>& onDone) override;
        void OnRequestTelemetry(const infra::Function<void()>& onDone) override;
        void OnSetEncoderResolution(uint32_t resolution, const infra::Function<void()>& onDone) override;
        void OnConfigureTelemetryRate(uint32_t rateHz, const infra::Function<void()>& onDone) override;
        void OnSelectAlgorithm(FocControlLoop loop, uint8_t algorithm, const infra::Function<void(const FocAlgorithms&)>& onDone) override;
        void OnQueryAlgorithms(const infra::Function<void(const FocAlgorithms&)>& onDone) override;
        void OnSetParameter(FocParameter parameter, float value, const infra::Function<void(FocParameterValue)>& onDone) override;
        void OnQueryParameter(FocParameter parameter, const infra::Function<void(FocParameterValue)>& onDone) override;

    private:
        static FocFaultCode ToCanFaultCode(state_machine::FaultCode code);
        static FocMotorState ToCanMotorState(const state_machine::State& state, bool hasPartialCalibration);

        void ReportCommandOutcome(uint8_t commandId, state_machine::CommandResult result,
            const infra::Function<void(services::CanAckStatus)>& onDone);
        void ReportSelectFailure(state_machine::SelectResult result);
        bool SetpointRejected(uint8_t commandId, state_machine::ControlMode requiredMode, bool withinRange);
        bool SetpointAllowedInCurrentState() const;
        bool ReserveCalibration(uint8_t commandId);
        bool ElectricalIdentificationInProgress() const;
        bool MechanicalIdentificationInProgress() const;
        void FailElectricalIdentification(FocMotorCategoryError error);
        bool SettingOutstanding() const;
        void SetLegacyBandwidth(uint8_t messageId, FocParameter parameter, float bandwidth, const infra::Function<void()>& onDone);
        void SetLoopSetting(FocParameter parameter, float value, const state_machine::ControlModeStateMachine::OnSetting& completed);
        void SetStartupSetting(FocParameter parameter, uint32_t value);
        void CompleteAlgorithmSelection(state_machine::SettingResult result);
        void CompleteParameter(state_machine::SettingResult result);
        void CompleteStoredCommand(uint8_t commandId, state_machine::SettingResult result);
        void ReportSettingFailure(uint8_t commandId, state_machine::SettingResult result);
        FocAlgorithms ActiveAlgorithms() const;
        FocParameterValue HeldValue(FocParameter parameter) const;
        FocParameterValue HeldLoopSetting(FocParameter parameter) const;
        FocParameterValue HeldMotorParameter(FocParameter parameter) const;

        FocMotorCategoryServer& server;
        state_machine::ControlModeStateMachine& controlMode;
        const drivers::ThreePhaseInverter& inverter;
        drivers::Encoder& encoder;
        services::ElectricalParametersIdentification& electricalIdent;
        services::MechanicalParametersIdentification* mechIdent;

        infra::Function<void(FocMotorMode)> pendingSelectCallback;
        infra::Function<void(services::CanAckStatus)> pendingAlignDoneCallback;
        infra::Function<void()> pendingElectricalIdentDoneCallback;
        infra::Function<void()> pendingMechIdentDoneCallback;
        infra::Function<void()> pendingNvmDoneCallback;
        infra::Function<void(const FocAlgorithms&)> pendingAlgorithmCallback;
        infra::Function<void(FocParameterValue)> pendingParameterCallback;
        FocParameter pendingParameter{ FocParameter::currentBandwidth };
        uint32_t previousStartupSetting{ 0 };
        uint8_t pendingLegacyCommand{ 0 };
        std::optional<foc::Ohm> pendingElectricalR;
        std::optional<foc::MilliHenry> pendingElectricalL;
    };
}
