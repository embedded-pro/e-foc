#include "core/can/FocMotorCanBridge.hpp"
#include "core/can/FocMotorCommandOutcome.hpp"
#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/math/TravelLimits.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "e-foc-protocol/foc/ParameterValidation.hpp"
#include "numerical/math/Math.hpp"
#include <cmath>
#include <variant>

namespace can
{
    namespace
    {
        FocMotorState ToCanMotorState(const state_machine::State& state, bool hasPartialCalibration)
        {
            if (std::holds_alternative<state_machine::Fault>(state))
                return FocMotorState::fault;
            if (std::holds_alternative<state_machine::Calibrating>(state))
                return FocMotorState::calibrating;
            if (std::holds_alternative<state_machine::Enabled>(state))
                return FocMotorState::running;
            if (std::holds_alternative<state_machine::Stopping>(state))
                return FocMotorState::stopping;
            if (std::holds_alternative<state_machine::Ready>(state))
                return FocMotorState::ready;
            if (hasPartialCalibration)
                return FocMotorState::partialCalibration;
            return FocMotorState::idle;
        }

        void ReportSelectFailure(FocMotorCategoryServer& server, state_machine::SelectResult result)
        {
            if (result == state_machine::SelectResult::invalidMode)
                server.SendCommandAck(can::focSelectControlModeId, services::CanAckStatus::invalidPayload);
            else
                server.SendCategoryError(can::focSelectControlModeId, state_machine::ToCategoryError(result));
        }

        bool SetpointAllowed(const state_machine::State& state)
        {
            return std::holds_alternative<state_machine::Ready>(state) || std::holds_alternative<state_machine::Enabled>(state);
        }
    }

    FocMotorCanBridge::FocMotorCanBridge(
        FocMotorCategoryServer& server,
        state_machine::ControlModeStateMachine& controlMode,
        const Platform& platform,
        const Services& motorServices,
        services::Tracer& tracer)
        : FocMotorCategoryServerObserver(server)
        , server(server)
        , controlMode(controlMode)
        , inverter(platform.inverter)
        , encoder(platform.encoder)
        , commissioning(server, controlMode, motorServices.electricalIdent, motorServices.currentOffsets, motorServices.standstill)
        , settings(server, controlMode, platform.diagnostics, platform.resetCause, platform.firmware, motorServices.firmwareUpgrade)
        , firmwareUpgrade(motorServices.firmwareUpgrade)
        , faultHistory(motorServices.faultHistory)
    {
        tracer.Trace() << "FocMotorCanBridge: initialised";
    }

    void FocMotorCanBridge::BroadcastFault(state_machine::FaultCode code)
    {
        server.BroadcastFaultStatus(ToCanFaultCode(code));
    }

    FocFaultCode FocMotorCanBridge::ToCanFaultCode(state_machine::FaultCode code)
    {
        switch (code)
        {
            case state_machine::FaultCode::none:
                return FocFaultCode::none;
            case state_machine::FaultCode::overcurrent:
                return FocFaultCode::overCurrent;
            case state_machine::FaultCode::overvoltage:
                return FocFaultCode::overVoltage;
            case state_machine::FaultCode::undervoltage:
                return FocFaultCode::underVoltage;
            case state_machine::FaultCode::overtemperature:
                return FocFaultCode::overTemperature;
            case state_machine::FaultCode::encoderLoss:
                return FocFaultCode::sensorFault;
            case state_machine::FaultCode::stall:
                return FocFaultCode::stall;
            case state_machine::FaultCode::phaseMissing:
                return FocFaultCode::phaseMissing;
            case state_machine::FaultCode::hardwareFault:
                return FocFaultCode::hardwareFault;
            case state_machine::FaultCode::calibrationFailed:
                return FocFaultCode::calibrationFailed;
            case state_machine::FaultCode::overspeed:
                return FocFaultCode::overSpeed;
            case state_machine::FaultCode::followingError:
                return FocFaultCode::followingError;
            case state_machine::FaultCode::commandTimeout:
                return FocFaultCode::commandTimeout;
            case state_machine::FaultCode::positionLimit:
                return FocFaultCode::positionLimit;
        }

        return FocFaultCode::none;
    }

    void FocMotorCanBridge::ObserveCommands(const infra::Function<void()>& onAccepted)
    {
        commandAccepted = onAccepted;
    }

    void FocMotorCanBridge::ReportTargetReached(const infra::Function<bool()>& isReached)
    {
        settings.ReportTargetReached(isReached);
    }

    void FocMotorCanBridge::NotifyCommandAccepted()
    {
        if (commandAccepted != nullptr)
            commandAccepted();
    }

    void FocMotorCanBridge::OnStart(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        if (pendingStartCallback != nullptr)
        {
            server.SendCategoryError(can::focStartId, FocMotorCategoryError::busy);
            return;
        }

        pendingStartCallback = onDone;
        controlMode.Drive().ActiveStateMachine().CmdEnableAsync([this](state_machine::CommandResult result)
            {
                auto callback = pendingStartCallback;
                pendingStartCallback = nullptr;
                if (result == state_machine::CommandResult::ok)
                    NotifyCommandAccepted();
                ReportCommandOutcome(server, can::focStartId, result, callback);
            });
    }

    void FocMotorCanBridge::OnStop(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        ReportCommandOutcome(server, can::focStopId, controlMode.Drive().ActiveStateMachine().CmdDisable(), onDone);
    }

    void FocMotorCanBridge::OnClearFault(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        ReportCommandOutcome(server, can::focClearFaultId, controlMode.Drive().ActiveStateMachine().CmdClearFault(), onDone);
    }

    void FocMotorCanBridge::OnEmergencyStop(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        if (firmwareUpgrade)
            firmwareUpgrade->get().CancelDownload();

        ReportCommandOutcome(server, can::focEmergencyStopId, controlMode.Drive().ActiveStateMachine().CmdEmergencyStop(), onDone);
    }

    void FocMotorCanBridge::OnQuickStop(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        ReportCommandOutcome(server, can::focQuickStopId, controlMode.Drive().ActiveStateMachine().CmdQuickStop(), onDone);
    }

    void FocMotorCanBridge::OnSelectControlMode(FocMotorMode mode,
        const infra::Function<void(FocMotorMode)>& onActivated)
    {
        if (pendingSelectCallback != nullptr)
        {
            server.SendCategoryError(can::focSelectControlModeId, FocMotorCategoryError::busy);
            return;
        }

        const auto mapped = state_machine::FromCanMode(mode);
        if (!mapped.has_value())
        {
            server.SendCommandAck(can::focSelectControlModeId, services::CanAckStatus::invalidPayload);
            return;
        }

        pendingSelectCallback = onActivated;
        controlMode.Modes().Select(*mapped, [this](state_machine::SelectResult result)
            {
                auto callback = pendingSelectCallback;
                pendingSelectCallback = nullptr;
                if (result == state_machine::SelectResult::ok)
                    callback(state_machine::ToCanMode(controlMode.Drive().Active()));
                else
                    ReportSelectFailure(server, result);
            });
    }

    void FocMotorCanBridge::OnSetSetpoint(const FocSetpoint& setpoint, const infra::Function<void()>& onDone)
    {
        const auto commandId = SetpointCommandId(setpoint.kind);

        switch (setpoint.kind)
        {
            case FocSetpointKind::torqueCurrent:
                ApplyTorqueSetpoint(commandId, foc::Ampere{ setpoint.value }, onDone);
                break;
            case FocSetpointKind::torqueNewtonMeter:
                ApplyTorqueInNewtonMeter(commandId, foc::NewtonMeter{ setpoint.value }, onDone);
                break;
            case FocSetpointKind::speed:
                ApplySpeedSetpoint(commandId, foc::RadiansPerSecond{ setpoint.value }, onDone);
                break;
            case FocSetpointKind::position:
                ApplyPositionSetpoint(commandId, foc::Radians{ setpoint.value }, onDone);
                break;
        }
    }

    void FocMotorCanBridge::ApplyTorqueInNewtonMeter(uint8_t commandId, foc::NewtonMeter torque, const infra::Function<void()>& onDone)
    {
        const auto torqueConstant = foc::TorqueConstantFor(controlMode.Drive().CalibrationRecord().polePairs, controlMode.Drive().ActiveCommon().Calibration().ActiveFluxLinkage());
        if (!foc::IsFinitePositive(torqueConstant.Value()))
        {
            server.SendCommandAck(commandId, services::CanAckStatus::invalidState);
            return;
        }

        ApplyTorqueSetpoint(commandId, foc::Ampere{ torque.Value() / torqueConstant.Value() }, onDone);
    }

    void FocMotorCanBridge::ApplyTorqueSetpoint(uint8_t commandId, foc::Ampere value, const infra::Function<void()>& onDone)
    {
        if (SetpointRejected(commandId, state_machine::ControlMode::torque,
                math::Abs(value.Value()) <= inverter.MaxCurrentSupported().Value()))
            return;

        if (controlMode.Commands().TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, value }))
        {
            NotifyCommandAccepted();
            onDone();
        }
        else
            server.SendCategoryError(commandId, FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::ApplySpeedSetpoint(uint8_t commandId, foc::RadiansPerSecond value, const infra::Function<void()>& onDone)
    {
        if (SetpointRejected(commandId, state_machine::ControlMode::speed,
                math::Abs(value.Value()) <= maxSpeedSetpoint))
            return;

        if (controlMode.Commands().TrySetSpeed(value))
        {
            NotifyCommandAccepted();
            onDone();
        }
        else
            server.SendCategoryError(commandId, FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::ApplyPositionSetpoint(uint8_t commandId, foc::Radians value, const infra::Function<void()>& onDone)
    {
        const foc::TravelLimits limits{ foc::Radians{ controlMode.Config().limits.positionMinimum }, foc::Radians{ controlMode.Config().limits.positionMaximum } };
        if (SetpointRejected(commandId, state_machine::ControlMode::position,
                math::Abs(value.Value()) <= maxPositionSetpoint && limits.Admits(value)))
            return;

        if (controlMode.Commands().TrySetPosition(value))
        {
            NotifyCommandAccepted();
            onDone();
        }
        else
            server.SendCategoryError(commandId, FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnAlign(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        commissioning.Align(onDone);
    }

    void FocMotorCanBridge::OnIdentifyElectrical(const infra::Function<void()>& onDone)
    {
        commissioning.IdentifyElectrical(onDone);
    }

    void FocMotorCanBridge::OnCalibrate(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        commissioning.Calibrate(onDone);
    }

    void FocMotorCanBridge::OnClearCalibration(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        commissioning.ClearCalibration(onDone);
    }

    void FocMotorCanBridge::OnClearDiagnostics(const infra::Function<void()>& onDone)
    {
        settings.ClearDiagnostics();
        onDone();
    }

    static_assert(services::FaultHistoryData::capacity == focFaultHistoryDepth);

    void FocMotorCanBridge::OnQueryFaultHistory(uint8_t age, const infra::Function<void(const FocFaultRecord&)>& onDone)
    {
        if (!faultHistory.has_value())
        {
            onDone(FocFaultRecord{ .age = age });
            return;
        }

        faultHistory->get().WhenLoaded([this, age, onDone]()
            {
                AnswerFaultHistory(age, onDone);
            });
    }

    void FocMotorCanBridge::AnswerFaultHistory(uint8_t age, const infra::Function<void(const FocFaultRecord&)>& onDone)
    {
        FocFaultRecord answer{ .age = age };
        const auto record = faultHistory->get().OfAge(age);

        if (record.has_value())
            answer = FocFaultRecord{ age, static_cast<FocFaultCode>(record->code), static_cast<FocMotorMode>(record->mode), record->uptimeMs,
                static_cast<int16_t>(record->powerUp), foc::Volts{ record->busVoltageCentivolts / 100.0f }, foc::Ampere{ record->peakCurrentCentiamperes / 100.0f } };

        onDone(answer);
    }

    void FocMotorCanBridge::OnClearFaultHistory(const infra::Function<void(bool stored)>& onDone)
    {
        if (!faultHistory.has_value())
        {
            onDone(true);
            return;
        }

        faultHistory->get().WhenLoaded([this, onDone]()
            {
                faultHistory->get().Clear(onDone);
            });
    }

    void FocMotorCanBridge::OnRecalibrate(const infra::Function<void()>& onAccepted)
    {
        settings.Recalibrate(onAccepted);
    }

    void FocMotorCanBridge::OnResetModel(const infra::Function<void(bool stored)>& onDone)
    {
        settings.ResetModel(onDone);
    }

    void FocMotorCanBridge::OnRequestTelemetry(const infra::Function<void()>& onDone)
    {
        const auto& state = controlMode.Drive().ActiveStateMachine().CurrentState();
        const auto faultCode = std::holds_alternative<state_machine::Fault>(state)
                                   ? ToCanFaultCode(controlMode.Drive().ActiveStateMachine().LastFaultCode())
                                   : FocFaultCode::none;

        server.BroadcastTelemetryStatus(ToCanMotorState(state, controlMode.Drive().ActiveStateMachine().HasPartialCalibration()), faultCode);
        server.BroadcastTelemetryMotion(controlMode.Drive().ActiveStateMachine().MeasuredSpeed(), encoder.Read());
        onDone();
    }

    void FocMotorCanBridge::OnSelectAlgorithm(FocControlLoop loop, uint8_t algorithm, const infra::Function<void(const FocAlgorithms&)>& onDone)
    {
        settings.SelectAlgorithm(loop, algorithm, onDone);
    }

    void FocMotorCanBridge::OnQueryAlgorithms(const infra::Function<void(const FocAlgorithms&)>& onDone)
    {
        onDone(settings.ActiveAlgorithms());
    }

    void FocMotorCanBridge::OnSetParameter(FocParameter parameter, float value, const infra::Function<void(FocParameterValue)>& onDone)
    {
        settings.SetParameter(parameter, value, onDone);
    }

    void FocMotorCanBridge::OnQueryParameter(FocParameter parameter, const infra::Function<void(FocParameterValue)>& onDone)
    {
        onDone(settings.HeldValue(parameter));
    }

    bool FocMotorCanBridge::SetpointRejected(uint8_t commandId, state_machine::ControlMode requiredMode,
        bool withinRange)
    {
        if (controlMode.Drive().Active() != requiredMode)
        {
            server.SendCategoryError(commandId, FocMotorCategoryError::modeMismatch);
            return true;
        }

        if (!withinRange)
        {
            server.SendCommandAck(commandId, services::CanAckStatus::invalidPayload);
            return true;
        }

        if (!SetpointAllowed(controlMode.Drive().ActiveStateMachine().CurrentState()))
        {
            server.SendCommandAck(commandId, services::CanAckStatus::invalidState);
            return true;
        }

        return false;
    }
}
