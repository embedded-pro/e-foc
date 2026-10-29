#pragma once

#include "core/can/FocMotorCategoryServer.hpp"
#include "core/services/current_offsets/PhaseCurrentOffsets.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "core/services/standstill/RotorStandstill.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "infra/util/Function.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

namespace can
{
    class FocMotorCommissioningBridge
    {
    public:
        FocMotorCommissioningBridge(FocMotorCategoryServer& server, state_machine::ControlModeStateMachine& controlMode,
            services::ElectricalParametersIdentification& electricalIdent, services::MechanicalParametersIdentification* mechIdent,
            std::optional<std::reference_wrapper<services::PhaseCurrentOffsets>> currentOffsets = std::nullopt,
            std::optional<std::reference_wrapper<services::RotorStandstill>> standstill = std::nullopt);

        void Align(const infra::Function<void(services::CanAckStatus)>& onDone);
        void IdentifyElectrical(const infra::Function<void()>& onDone);
        void IdentifyMechanical(const infra::Function<void()>& onDone);
        void Calibrate(const infra::Function<void(services::CanAckStatus)>& onDone);
        void ClearCalibration(const infra::Function<void(services::CanAckStatus)>& onDone);
        void AbortMechanicalIdentification();

    private:
        bool MaintenanceOutstanding() const;
        bool ReserveCalibration(uint8_t commandId);
        bool ElectricalIdentificationInProgress() const;
        bool MechanicalIdentificationInProgress() const;
        void MeasureCurrentOffsets();
        void EstimateResistanceAndInductance();
        void OnResistanceAndInductance(const services::ElectricalParametersIdentification::ResistanceInductanceResult& result);
        void OnPolePairs(std::optional<std::size_t> polePairs);
        void FailElectricalIdentification(FocMotorCategoryError error);

        FocMotorCategoryServer& server;
        state_machine::ControlModeStateMachine& controlMode;
        services::ElectricalParametersIdentification& electricalIdent;
        services::MechanicalParametersIdentification* mechIdent;
        std::optional<std::reference_wrapper<services::PhaseCurrentOffsets>> currentOffsets;
        std::optional<std::reference_wrapper<services::RotorStandstill>> standstill;

        infra::Function<void(services::CanAckStatus)> pendingAlignDoneCallback;
        infra::Function<void()> pendingElectricalIdentDoneCallback;
        infra::Function<void()> pendingMechIdentDoneCallback;
        infra::Function<void(services::CanAckStatus)> pendingCalibrateCallback;
        infra::Function<void(services::CanAckStatus)> pendingClearCalibrationCallback;
        std::optional<foc::Ohm> pendingElectricalR;
        std::optional<foc::MilliHenry> pendingElectricalL;
        foc::PhaseCurrents pendingOffsets{};
    };
}
