#pragma once

#include "core/can/FocMotorCategoryServer.hpp"
#include "core/can/FocMotorCommissioningBridge.hpp"
#include "core/can/FocMotorSettingsBridge.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/platform_abstraction/PlatformDiagnostics.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "core/services/fault_history/FaultLog.hpp"
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
            std::optional<std::reference_wrapper<services::FaultLog>> faultHistory{};
        };

        FocMotorCanBridge(
            FocMotorCategoryServer& server,
            state_machine::ControlModeStateMachine& controlMode,
            const Platform& platform,
            const Services& motorServices,
            services::Tracer& tracer);

        static constexpr float maxPositionSetpoint{ foc::CommandLimits::maxPositionSetpoint };
        static constexpr float maxSpeedSetpoint{ foc::CommandLimits::maxSpeedSetpoint };

        static FocFaultCode ToCanFaultCode(state_machine::FaultCode code);

        void BroadcastFault(state_machine::FaultCode code);
        void ObserveCommands(const infra::Function<void()>& onAccepted);
        void ReportTargetReached(const infra::Function<bool()>& isReached);

        void OnStart(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnStop(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnClearFault(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnEmergencyStop(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnSelectControlMode(FocMotorMode mode, const infra::Function<void(FocMotorMode)>& onActivated) override;
        void OnSetSetpoint(const FocSetpoint& setpoint, const infra::Function<void()>& onDone) override;

        void OnAlign(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnIdentifyElectrical(const infra::Function<void()>& onDone) override;
        void OnRequestTelemetry(const infra::Function<void()>& onDone) override;
        void OnSelectAlgorithm(FocControlLoop loop, uint8_t algorithm, const infra::Function<void(const FocAlgorithms&)>& onDone) override;
        void OnQueryAlgorithms(const infra::Function<void(const FocAlgorithms&)>& onDone) override;
        void OnSetParameter(FocParameter parameter, float value, const infra::Function<void(FocParameterValue)>& onDone) override;
        void OnQueryParameter(FocParameter parameter, const infra::Function<void(FocParameterValue)>& onDone) override;
        void OnCalibrate(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnClearCalibration(const infra::Function<void(services::CanAckStatus)>& onDone) override;
        void OnClearDiagnostics(const infra::Function<void()>& onDone) override;
        void OnQueryFaultHistory(uint8_t age, const infra::Function<void(const FocFaultRecord&)>& onDone) override;
        void OnClearFaultHistory(const infra::Function<void(bool stored)>& onDone) override;
        void OnRecalibrate(const infra::Function<void()>& onAccepted) override;
        void OnResetModel(const infra::Function<void(bool stored)>& onDone) override;

    private:
        void ApplyTorqueSetpoint(uint8_t commandId, foc::Ampere value, const infra::Function<void()>& onDone);
        void ApplyTorqueInNewtonMeter(uint8_t commandId, foc::NewtonMeter torque, const infra::Function<void()>& onDone);
        void ApplySpeedSetpoint(uint8_t commandId, foc::RadiansPerSecond value, const infra::Function<void()>& onDone);
        void ApplyPositionSetpoint(uint8_t commandId, foc::Radians value, const infra::Function<void()>& onDone);
        bool SetpointRejected(uint8_t commandId, state_machine::ControlMode requiredMode, bool withinRange);
        void NotifyCommandAccepted();

        FocMotorCategoryServer& server;
        state_machine::ControlModeStateMachine& controlMode;
        const drivers::ThreePhaseInverter& inverter;
        drivers::Encoder& encoder;
        FocMotorCommissioningBridge commissioning;
        FocMotorSettingsBridge settings;
        FocMotorSettingsBridge::FirmwareUpgrade firmwareUpgrade;
        std::optional<std::reference_wrapper<services::FaultLog>> faultHistory;

        infra::Function<void(FocMotorMode)> pendingSelectCallback;
        infra::Function<void(services::CanAckStatus)> pendingStartCallback;
        infra::Function<void()> commandAccepted;
    };
}
