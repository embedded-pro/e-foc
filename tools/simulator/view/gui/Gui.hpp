#pragma once

#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "tools/simulator/model/Model.hpp"
#include "tools/simulator/view/gui/ControlPanel.hpp"
#include "tools/simulator/view/gui/ParametersPanel.hpp"
#include "tools/simulator/view/gui/ScopesPanel.hpp"
#include <QMainWindow>
#include <cstddef>

namespace simulator
{
    class Gui
        : public QMainWindow
        , public ThreePhaseMotorModelObserver
    {
        Q_OBJECT

    public:
        Gui(ThreePhaseMotorModel& model, foc::Controllable& controller, infra::EventDispatcherWithWeakPtr& eventDispatcher,
            const ThreePhaseMotorModel::Parameters& motorParameters, const ParametersPanel::PidParameters& pidParameters,
            const ControlPanel::SetpointConfig& setpointConfig, foc::Volts powerSupplyVoltage, QWidget* parent = nullptr);

        // ThreePhaseMotorModelObserver
        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta) override;
        void StatorVoltages(foc::ThreePhase phaseVoltages, foc::TwoPhase alphaBeta) override;
        void Finished() override;

    signals:
        void setpointChanged(int value);
        void alignRequested();
        void identifyElectricalRequested();
        void identifyMechanicalRequested();

    public:
        void UpdatePidParameters(const ParametersPanel::PidParameters& pidParameters);
        void SetIdentifiedElectrical(foc::Ohm resistance, foc::MilliHenry inductance);
        void SetIdentifiedPolePairs(std::size_t polePairs);
        void SetIdentifiedMechanical(foc::NewtonMeterSecondPerRadian friction, foc::NewtonMeterSecondSquared inertia);
        void SetAlignmentOffset(foc::Radians offset);
        void CalibrationFinished();
        void DisableMechanicalIdent();

    private:
        ThreePhaseMotorModel& model;
        foc::Controllable& controller;
        infra::EventDispatcherWithWeakPtr& eventDispatcher;

        ControlPanel* controlPanel;
        ParametersPanel* parametersPanel;
        ScopesPanel* scopesPanel;
    };
}
