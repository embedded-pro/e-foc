#pragma once

#include "core/can/FocMotorCategoryServer.hpp"
#include "core/can/FocMotorCommissioningBridge.hpp"
#include "core/can/FocMotorSettingsBridge.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/platform_abstraction/PlatformDiagnostics.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "infra/util/Function.hpp"
#include "services/tracer/Tracer.hpp"
#include <functional>
#include <optional>

namespace can
{
    class FocMotorCanBridge
        : public FocMotorCategoryServerObserver
    {
    public:
        struct Platform
        {
            const drivers::ThreePhaseInverter& inverter;
            drivers::Encoder& encoder;
            application::PlatformDiagnostics& diagnostics;
            FocResetCause resetCause;
            FocFirmwareIdentity firmware;
        };

        struct Services
        {
            services::ElectricalParametersIdentification& electricalIdent;
            std::optional<std::reference_wrapper<services::PhaseCurrentOffsets>> currentOffsets;
            std::optional<std::reference_wrapper<services::RotorStandstill>> standstill;
            FocMotorSettingsBridge::FirmwareUpgrade firmwareUpgrade;
        };

        FocMotorCanBridge(
            FocMotorCategoryServer& server,
            state_machine::ControlModeStateMachine& controlMode,
            const Platform& platform,
            const Services& motorServices,
            services::Tracer& tracer);

        static constexpr float maxPositionSetpoint{ foc::CommandLimits::maxPositionSetpoint };
        static constexpr float maxSpeedSetpoint{ foc::CommandLimits::maxSpeedSetpoint };

        void BroadcastFault(state_machine::FaultCode code);
        void ObserveCommands(const infra::Function<void()>& onAccepted);
        void ReportTargetReached(const infra::Function<bool()>& isReached);

        void OnStart(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnStop(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnClearFault(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnEmergencyStop(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnQuickStop(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnSelectControlMode(FocMotorMode mode, const infra::Function<void(FocMotorMode)>& onActivated) override;
        void OnSetTorqueSetpoint(foc::Ampere value, const infra::Function<void()>& onDone) override;
        void OnSetSpeedSetpoint(foc::RadiansPerSecond value, const infra::Function<void()>& onDone) override;
        void OnSetPositionSetpoint(foc::Radians value, const infra::Function<void()>& onDone) override;

        void OnSetPidCurrent(float bandwidth, const infra::Function<void()>& onDone) override;
        void OnSetPidSpeed(float bandwidth, const infra::Function<void()>& onDone) override;
        void OnSetPidPosition(float bandwidth, const infra::Function<void()>& onDone) override;
        void OnAlign(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnIdentifyElectrical(const infra::Function<void()>& onDone) override;
        void OnRequestTelemetry(const infra::Function<void()>& onDone) override;
        void OnSetEncoderResolution(uint32_t resolution, const infra::Function<void()>& onDone) override;
        void OnConfigureTelemetryRate(uint32_t rateHz, const infra::Function<void()>& onDone) override;
        void OnSelectAlgorithm(FocControlLoop loop, uint8_t algorithm, const infra::Function<void(const FocAlgorithms&)>& onDone) override;
        void OnQueryAlgorithms(const infra::Function<void(const FocAlgorithms&)>& onDone) override;
        void OnSetParameter(FocParameter parameter, float value, const infra::Function<void(FocParameterValue)>& onDone) override;
        void OnQueryParameter(FocParameter parameter, const infra::Function<void(FocParameterValue)>& onDone) override;
        void OnCalibrate(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnClearCalibration(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnClearDiagnostics(const infra::Function<void()>& onDone) override;

    private:
        static FocFaultCode ToCanFaultCode(state_machine::FaultCode code);
        static FocMotorState ToCanMotorState(const state_machine::State& state, bool hasPartialCalibration);

        void ReportSelectFailure(state_machine::SelectResult result);
        bool SetpointRejected(uint8_t commandId, state_machine::ControlMode requiredMode, bool withinRange);
        bool SetpointAllowedInCurrentState() const;
        void NotifyCommandAccepted();

        FocMotorCategoryServer& server;
        state_machine::ControlModeStateMachine& controlMode;
        const drivers::ThreePhaseInverter& inverter;
        drivers::Encoder& encoder;
        FocMotorCommissioningBridge commissioning;
        FocMotorSettingsBridge settings;
        FocMotorSettingsBridge::FirmwareUpgrade firmwareUpgrade;

        infra::Function<void(FocMotorMode)> pendingSelectCallback;
        infra::Function<void(services::CanAckStatus)> pendingStartCallback;
        infra::Function<void()> commandAccepted;
    };
}
