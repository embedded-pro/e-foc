#include "core/can/FocMotorCanBridge.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include <cmath>

namespace can
{
    FocMotorCanBridge::FocMotorCanBridge(
        FocMotorCategoryServer& server,
        state_machine::ControlModeStateMachine& controlMode,
        const drivers::ThreePhaseInverter& inverter,
        services::ElectricalParametersIdentification& electricalIdent,
        services::MechanicalParametersIdentification* mechIdent,
        foc::NewtonMeter mechTorqueConstant,
        services::NonVolatileMemory& nvm,
        services::ConfigData configData,
        services::Tracer& tracer)
        : FocMotorCategoryServerObserver(server)
        , server(server)
        , controlMode(controlMode)
        , inverter(inverter)
        , electricalIdent(electricalIdent)
        , mechIdent{ mechIdent }
        , mechTorqueConstant{ mechTorqueConstant }
        , nvm(nvm)
        , configData(configData)
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
            case state_machine::FaultCode::overcurrent:
                return FocFaultCode::overCurrent;
            case state_machine::FaultCode::overvoltage:
                return FocFaultCode::overVoltage;
            case state_machine::FaultCode::overtemperature:
                return FocFaultCode::overTemperature;
            case state_machine::FaultCode::encoderLoss:
                return FocFaultCode::sensorFault;
            default:
                return FocFaultCode::none;
        }
    }

    FocMotorState FocMotorCanBridge::ToCanMotorState(const state_machine::State& state)
    {
        if (std::holds_alternative<state_machine::Fault>(state))
            return FocMotorState::fault;
        if (std::holds_alternative<state_machine::Calibrating>(state))
            return FocMotorState::calibrating;
        if (std::holds_alternative<state_machine::Enabled>(state))
            return FocMotorState::running;
        return FocMotorState::idle;
    }

    void FocMotorCanBridge::OnStart(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        ReportCommandOutcome(can::focStartId, controlMode.ActiveStateMachine().CmdEnable(), onDone);
    }

    void FocMotorCanBridge::OnStop(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        ReportCommandOutcome(can::focStopId, controlMode.ActiveStateMachine().CmdDisable(), onDone);
    }

    void FocMotorCanBridge::OnClearFault(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        ReportCommandOutcome(can::focClearFaultId, controlMode.ActiveStateMachine().CmdClearFault(), onDone);
    }

    void FocMotorCanBridge::OnEmergencyStop(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        ReportCommandOutcome(can::focEmergencyStopId, controlMode.ActiveStateMachine().CmdEmergencyStop(), onDone);
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
        controlMode.Select(*mapped, [this](state_machine::SelectResult result)
            {
                auto callback = pendingSelectCallback;
                pendingSelectCallback = nullptr;
                if (result == state_machine::SelectResult::ok)
                    callback(state_machine::ToCanMode(controlMode.Active()));
                else
                    ReportSelectFailure(result);
            });
    }

    void FocMotorCanBridge::OnSetTorqueSetpoint(foc::Ampere value, const infra::Function<void()>& onDone)
    {
        if (SetpointRejected(can::focSetTorqueSetpointId, state_machine::ControlMode::torque,
                std::fabs(value.Value()) <= inverter.MaxCurrentSupported().Value()))
            return;

        if (controlMode.TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, value }))
            onDone();
        else
            server.SendCategoryError(can::focSetTorqueSetpointId, FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnSetSpeedSetpoint(foc::RadiansPerSecond value, const infra::Function<void()>& onDone)
    {
        if (SetpointRejected(can::focSetSpeedSetpointId, state_machine::ControlMode::speed,
                std::fabs(value.Value()) <= maxSpeedSetpoint))
            return;

        if (controlMode.TrySetSpeed(value))
            onDone();
        else
            server.SendCategoryError(can::focSetSpeedSetpointId, FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnSetPositionSetpoint(foc::Radians value, const infra::Function<void()>& onDone)
    {
        if (SetpointRejected(can::focSetPositionSetpointId, state_machine::ControlMode::position,
                std::fabs(value.Value()) <= maxPositionSetpoint))
            return;

        if (controlMode.TrySetPosition(value))
            onDone();
        else
            server.SendCategoryError(can::focSetPositionSetpointId, FocMotorCategoryError::modeMismatch);
    }

    void FocMotorCanBridge::OnSetPidCurrent(float bandwidth, const infra::Function<void()>& onDone)
    {
        if (!controlMode.TrySetCurrentBandwidth(bandwidth))
        {
            server.SendCommandAck(can::focSetPidCurrentId, services::CanAckStatus::invalidPayload);
            return;
        }
        onDone();
    }

    void FocMotorCanBridge::OnSetPidSpeed(float bandwidth, const infra::Function<void()>& onDone)
    {
        if (!controlMode.TrySetSpeedBandwidth(bandwidth))
        {
            server.SendCommandAck(can::focSetPidSpeedId, services::CanAckStatus::invalidPayload);
            return;
        }
        onDone();
    }

    void FocMotorCanBridge::OnSetPidPosition(float bandwidth, const infra::Function<void()>& onDone)
    {
        if (!controlMode.TrySetPositionBandwidth(bandwidth))
        {
            server.SendCommandAck(can::focSetPidPositionId, services::CanAckStatus::invalidPayload);
            return;
        }
        onDone();
    }

    void FocMotorCanBridge::OnIdentifyElectrical(const infra::Function<void()>& onDone)
    {
        if (pendingElectricalIdentDoneCallback != nullptr)
        {
            server.SendCategoryError(can::focIdentifyElectricalId, FocMotorCategoryError::busy);
            return;
        }

        pendingElectricalIdentDoneCallback = onDone;

        electricalIdent.EstimateResistanceAndInductance({},
            [this](services::ElectricalParametersIdentification::ResistanceInductanceResult result)
            {
                if (!result.resistance.has_value() || !result.inductance.has_value() || result.fitQuality < 0.5f)
                {
                    pendingElectricalIdentDoneCallback = nullptr;
                    server.SendCategoryError(can::focIdentifyElectricalId, FocMotorCategoryError::calibrationFailed);
                    return;
                }

                pendingElectricalR = result.resistance;
                pendingElectricalL = result.inductance;

                electricalIdent.EstimateNumberOfPolePairs({},
                    [this](std::optional<std::size_t> polePairs)
                    {
                        auto callback = pendingElectricalIdentDoneCallback;
                        pendingElectricalIdentDoneCallback = nullptr;

                        if (!polePairs.has_value())
                        {
                            pendingElectricalR.reset();
                            pendingElectricalL.reset();
                            server.SendCategoryError(can::focIdentifyElectricalId, FocMotorCategoryError::calibrationFailed);
                            return;
                        }

                        server.BroadcastElectricalParams(*pendingElectricalR, *pendingElectricalL, *polePairs);
                        pendingElectricalR.reset();
                        pendingElectricalL.reset();
                        callback();
                    });
            });
    }

    void FocMotorCanBridge::OnIdentifyMechanical(const infra::Function<void()>& onDone)
    {
        if (mechIdent == nullptr)
        {
            server.SendCommandAck(can::focIdentifyMechanicalId, services::CanAckStatus::notImplemented);
            return;
        }

        if (pendingMechIdentDoneCallback != nullptr)
        {
            server.SendCategoryError(can::focIdentifyMechanicalId, FocMotorCategoryError::busy);
            return;
        }

        const auto& state = controlMode.ActiveStateMachine().CurrentState();
        const auto* ready = std::get_if<state_machine::Ready>(&state);
        if (ready == nullptr)
        {
            server.SendCommandAck(can::focIdentifyMechanicalId, services::CanAckStatus::invalidState);
            return;
        }

        const auto& cal = ready->loadedData;
        pendingMechIdentDoneCallback = onDone;

        mechIdent->EstimateFrictionAndInertia(
            mechTorqueConstant,
            static_cast<std::size_t>(cal.polePairs),
            {},
            [this](std::optional<foc::NewtonMeterSecondPerRadian> friction,
                std::optional<foc::NewtonMeterSecondSquared> inertia)
            {
                auto callback = pendingMechIdentDoneCallback;
                pendingMechIdentDoneCallback = nullptr;

                if (!friction.has_value() || !inertia.has_value())
                {
                    server.SendCategoryError(can::focIdentifyMechanicalId, FocMotorCategoryError::calibrationFailed);
                    return;
                }

                server.BroadcastMechanicalParams(*friction, *inertia);
                callback();
            });
    }

    void FocMotorCanBridge::OnRequestTelemetry(const infra::Function<void()>& onDone)
    {
        const auto& state = controlMode.ActiveStateMachine().CurrentState();
        const auto faultCode = std::holds_alternative<state_machine::Fault>(state)
                                   ? ToCanFaultCode(controlMode.ActiveStateMachine().LastFaultCode())
                                   : FocFaultCode::none;

        server.BroadcastTelemetryStatus(ToCanMotorState(state), faultCode);
        onDone();
    }

    void FocMotorCanBridge::OnSetEncoderResolution(uint32_t resolution, const infra::Function<void()>& onDone)
    {
        if (pendingNvmDoneCallback != nullptr)
        {
            server.SendCategoryError(can::focSetEncoderResolutionId, FocMotorCategoryError::busy);
            return;
        }

        if (resolution == 0)
        {
            server.SendCommandAck(can::focSetEncoderResolutionId, services::CanAckStatus::invalidPayload);
            return;
        }

        configData.encoderResolution = resolution;
        pendingNvmDoneCallback = onDone;

        nvm.SaveConfig(configData, [this](services::NvmStatus status)
            {
                auto callback = pendingNvmDoneCallback;
                pendingNvmDoneCallback = nullptr;
                if (status == services::NvmStatus::Ok)
                    callback();
                else
                    server.SendCategoryError(can::focSetEncoderResolutionId, FocMotorCategoryError::persistenceFailed);
            });
    }

    void FocMotorCanBridge::OnConfigureTelemetryRate(uint32_t rateHz, const infra::Function<void()>& onDone)
    {
        if (pendingNvmDoneCallback != nullptr)
        {
            server.SendCategoryError(can::focConfigureTelemetryRateId, FocMotorCategoryError::busy);
            return;
        }

        if (rateHz == 0 || rateHz > 10000)
        {
            server.SendCommandAck(can::focConfigureTelemetryRateId, services::CanAckStatus::invalidPayload);
            return;
        }

        configData.telemetryRateHz = rateHz;
        pendingNvmDoneCallback = onDone;

        nvm.SaveConfig(configData, [this](services::NvmStatus status)
            {
                auto callback = pendingNvmDoneCallback;
                pendingNvmDoneCallback = nullptr;
                if (status == services::NvmStatus::Ok)
                    callback();
                else
                    server.SendCategoryError(can::focConfigureTelemetryRateId, FocMotorCategoryError::persistenceFailed);
            });
    }

    void FocMotorCanBridge::ReportCommandOutcome(uint8_t commandId, state_machine::CommandResult result,
        const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        if (result == state_machine::CommandResult::ok)
            onDone(services::CanAckStatus::success);
        else if (result == state_machine::CommandResult::rejected)
            onDone(services::CanAckStatus::invalidState);
        else
            server.SendCategoryError(commandId, state_machine::ToCategoryError(result));
    }

    void FocMotorCanBridge::ReportSelectFailure(state_machine::SelectResult result)
    {
        if (result == state_machine::SelectResult::invalidMode)
            server.SendCommandAck(can::focSelectControlModeId, services::CanAckStatus::invalidPayload);
        else
            server.SendCategoryError(can::focSelectControlModeId, state_machine::ToCategoryError(result));
    }

    bool FocMotorCanBridge::SetpointRejected(uint8_t commandId, state_machine::ControlMode requiredMode,
        bool withinRange)
    {
        if (controlMode.Active() != requiredMode)
        {
            server.SendCategoryError(commandId, FocMotorCategoryError::modeMismatch);
            return true;
        }

        if (!withinRange)
        {
            server.SendCommandAck(commandId, services::CanAckStatus::invalidPayload);
            return true;
        }

        if (!SetpointAllowedInCurrentState())
        {
            server.SendCommandAck(commandId, services::CanAckStatus::invalidState);
            return true;
        }

        return false;
    }

    bool FocMotorCanBridge::SetpointAllowedInCurrentState() const
    {
        const auto& state = controlMode.ActiveStateMachine().CurrentState();
        return std::holds_alternative<state_machine::Ready>(state) ||
               std::holds_alternative<state_machine::Enabled>(state);
    }
}
