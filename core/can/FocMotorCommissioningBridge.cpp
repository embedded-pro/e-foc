#include "core/can/FocMotorCommissioningBridge.hpp"
#include "core/can/FocMotorCommandOutcome.hpp"
#include "core/foc/math/TorqueConstant.hpp"
#include <limits>
#include <variant>

namespace can
{
    FocMotorCommissioningBridge::FocMotorCommissioningBridge(FocMotorCategoryServer& server, state_machine::ControlModeStateMachine& controlMode,
        services::ElectricalParametersIdentification& electricalIdent, services::MechanicalParametersIdentification* mechIdent)
        : server(server)
        , controlMode(controlMode)
        , electricalIdent(electricalIdent)
        , mechIdent(mechIdent)
    {}

    void FocMotorCommissioningBridge::Align(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        if (pendingAlignDoneCallback != nullptr)
        {
            server.SendCategoryError(can::focAlignId, FocMotorCategoryError::busy);
            return;
        }

        pendingAlignDoneCallback = onDone;
        controlMode.CmdReAlign([this](state_machine::CommandResult result)
            {
                auto callback = pendingAlignDoneCallback;
                pendingAlignDoneCallback = nullptr;
                if (result == state_machine::CommandResult::ok)
                    callback(services::CanAckStatus::success);
                else
                    server.SendCategoryError(can::focAlignId, state_machine::ToCategoryError(result));
            });
    }

    void FocMotorCommissioningBridge::IdentifyElectrical(const infra::Function<void()>& onDone)
    {
        if (ElectricalIdentificationInProgress())
        {
            server.SendCategoryError(can::focIdentifyElectricalId, FocMotorCategoryError::busy);
            return;
        }

        if (!ReserveCalibration(can::focIdentifyElectricalId))
            return;

        pendingElectricalR.reset();
        pendingElectricalL.reset();
        pendingElectricalIdentDoneCallback = onDone;

        electricalIdent.EstimateResistanceAndInductance({},
            [this](services::ElectricalParametersIdentification::ResistanceInductanceResult result)
            {
                if (!result.resistance.has_value() || !result.inductance.has_value() || result.fitQuality < 0.5f)
                {
                    FailElectricalIdentification(FocMotorCategoryError::calibrationFailed);
                    return;
                }

                pendingElectricalR = result.resistance;
                pendingElectricalL = result.inductance;

                electricalIdent.EstimateNumberOfPolePairs({},
                    [this](std::optional<std::size_t> polePairs)
                    {
                        if (!polePairs.has_value() || *polePairs > std::numeric_limits<uint8_t>::max())
                        {
                            FailElectricalIdentification(FocMotorCategoryError::calibrationFailed);
                            return;
                        }

                        services::CalibrationData data{};
                        data.rPhase = pendingElectricalR->Value();
                        data.lD = pendingElectricalL->Value();
                        data.lQ = pendingElectricalL->Value();
                        data.polePairs = static_cast<uint8_t>(*polePairs);
                        data.fluxLinkage = controlMode.ActiveFluxLinkage().Value();
                        pendingElectricalR.reset();
                        pendingElectricalL.reset();

                        server.BroadcastElectricalParams(
                            foc::Ohm{ data.rPhase },
                            foc::MilliHenry{ data.lD },
                            static_cast<std::size_t>(data.polePairs));

                        controlMode.CmdCompleteExternalCalibration(data,
                            [this](state_machine::CommandResult result)
                            {
                                auto callback = pendingElectricalIdentDoneCallback;
                                pendingElectricalIdentDoneCallback = nullptr;

                                if (result == state_machine::CommandResult::ok)
                                    callback();
                                else
                                {
                                    if (result == state_machine::CommandResult::rejected)
                                        controlMode.CmdReleaseExternalCalibration();

                                    server.SendCategoryError(can::focIdentifyElectricalId,
                                        state_machine::ToCategoryError(result));
                                }
                            });
                    });
            });
    }

    void FocMotorCommissioningBridge::IdentifyMechanical(const infra::Function<void()>& onDone)
    {
        if (mechIdent == nullptr)
        {
            server.SendCommandAck(can::focIdentifyMechanicalId, services::CanAckStatus::notImplemented);
            return;
        }

        if (MechanicalIdentificationInProgress())
        {
            server.SendCategoryError(can::focIdentifyMechanicalId, FocMotorCategoryError::busy);
            return;
        }

        const auto cal = controlMode.ActiveCalibrationData();
        if (!cal.has_value())
        {
            server.SendCommandAck(can::focIdentifyMechanicalId, services::CanAckStatus::invalidState);
            return;
        }

        if (!ReserveCalibration(can::focIdentifyMechanicalId))
            return;

        pendingMechIdentDoneCallback = onDone;

        mechIdent->EstimateFrictionAndInertia(
            foc::TorqueConstantFor(cal->polePairs, controlMode.ActiveFluxLinkage()),
            static_cast<std::size_t>(cal->polePairs),
            {},
            [this](std::optional<foc::NewtonMeterSecondPerRadian> friction,
                std::optional<foc::NewtonMeterSecondSquared> inertia)
            {
                auto callback = pendingMechIdentDoneCallback;
                pendingMechIdentDoneCallback = nullptr;
                controlMode.CmdReleaseExternalCalibration();

                if (!friction.has_value() || !inertia.has_value())
                {
                    server.SendCategoryError(can::focIdentifyMechanicalId, FocMotorCategoryError::calibrationFailed);
                    return;
                }

                server.BroadcastMechanicalParams(*friction, *inertia);
                callback();
            });
    }

    void FocMotorCommissioningBridge::Calibrate(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        if (MaintenanceOutstanding())
        {
            server.SendCategoryError(focCalibrateId, FocMotorCategoryError::busy);
            return;
        }

        pendingCalibrateCallback = onDone;
        controlMode.ActiveStateMachine().CmdCalibrate([this](state_machine::CommandResult result)
            {
                auto callback = pendingCalibrateCallback;
                pendingCalibrateCallback = nullptr;
                ReportCommandOutcome(server, focCalibrateId, result, callback);
            });
    }

    void FocMotorCommissioningBridge::ClearCalibration(const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        if (MaintenanceOutstanding())
        {
            server.SendCategoryError(focClearCalibrationId, FocMotorCategoryError::busy);
            return;
        }

        pendingClearCalibrationCallback = onDone;
        controlMode.ActiveStateMachine().CmdClearCalibration([this](state_machine::CommandResult result)
            {
                auto callback = pendingClearCalibrationCallback;
                pendingClearCalibrationCallback = nullptr;
                ReportCommandOutcome(server, focClearCalibrationId, result, callback);
            });
    }

    void FocMotorCommissioningBridge::AbortMechanicalIdentification()
    {
        if (mechIdent != nullptr)
            mechIdent->Abort();
    }

    bool FocMotorCommissioningBridge::MaintenanceOutstanding() const
    {
        return pendingCalibrateCallback != nullptr || pendingClearCalibrationCallback != nullptr || controlMode.ActiveStateMachine().HasPendingAsyncWork() || controlMode.HasPendingConfigWrite();
    }

    bool FocMotorCommissioningBridge::ReserveCalibration(uint8_t commandId)
    {
        const auto reservation = controlMode.CmdReserveExternalCalibration();

        if (reservation == state_machine::CommandResult::ok)
            return true;

        if (reservation == state_machine::CommandResult::queued)
            server.SendCategoryError(commandId, FocMotorCategoryError::busy);
        else
            server.SendCommandAck(commandId, services::CanAckStatus::invalidState);

        return false;
    }

    // An emergency stop or a fault ends the reservation and aborts the estimation without answering it
    bool FocMotorCommissioningBridge::ElectricalIdentificationInProgress() const
    {
        return pendingElectricalIdentDoneCallback != nullptr && std::holds_alternative<state_machine::Calibrating>(controlMode.ActiveStateMachine().CurrentState());
    }

    bool FocMotorCommissioningBridge::MechanicalIdentificationInProgress() const
    {
        return mechIdent->IsRunning() || (pendingMechIdentDoneCallback != nullptr && std::holds_alternative<state_machine::Calibrating>(controlMode.ActiveStateMachine().CurrentState()));
    }

    void FocMotorCommissioningBridge::FailElectricalIdentification(FocMotorCategoryError error)
    {
        pendingElectricalR.reset();
        pendingElectricalL.reset();
        pendingElectricalIdentDoneCallback = nullptr;
        controlMode.CmdReleaseExternalCalibration();
        server.SendCategoryError(can::focIdentifyElectricalId, error);
    }
}
