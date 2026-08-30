#pragma once

#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "tools/simulator/view/gui/Gui.hpp"
#include <QObject>
#include <optional>

namespace simulator
{
    template<typename ControllerT>
    void WireCommonCalibrations(Gui& gui, ControllerT& controller,
        services::MotorAlignmentImpl& alignment, services::ElectricalParametersIdentificationImpl& electricalIdent,
        const ThreePhaseMotorModel::Parameters& motorParams)
    {
        QObject::connect(&gui, &Gui::alignRequested, [&]()
            {
                controller.Stop();
                gui.SetState(state_machine::Calibrating{ state_machine::CalibrationStep::alignment });
                alignment.ForceAlignment(motorParams.p, services::MotorAlignment::AlignmentConfig{},
                    [&gui](std::optional<foc::Radians> offset)
                    {
                        if (offset.has_value())
                            gui.SetAlignmentOffset(*offset);
                        gui.CalibrationFinished();
                        gui.SetState(state_machine::Ready{});
                    });
            });

        QObject::connect(&gui, &Gui::identifyElectricalRequested, [&]()
            {
                controller.Stop();
                gui.SetState(state_machine::Calibrating{ state_machine::CalibrationStep::resistanceAndInductance });
                electricalIdent.EstimateResistanceAndInductance(services::ElectricalParametersIdentification::ResistanceAndInductanceConfig{},
                    [&gui, &electricalIdent](services::ElectricalParametersIdentification::ResistanceInductanceResult result)
                    {
                        if (result.resistance.has_value() && result.inductance.has_value())
                            gui.SetState(state_machine::Calibrating{ state_machine::CalibrationStep::polePairs });
                        electricalIdent.EstimateNumberOfPolePairs(services::ElectricalParametersIdentification::PolePairsConfig{},
                            [&gui](std::optional<std::size_t> p)
                            {
                                if (p.has_value())
                                    gui.SetIdentifiedPolePairs(*p);
                                gui.CalibrationFinished();
                                gui.SetState(state_machine::Ready{});
                            });
                    });
            });
    }
}
