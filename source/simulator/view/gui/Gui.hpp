#pragma once

#include "foc/implementations/Runner.hpp"
#include "source/simulator/model/Model.hpp"
#include "source/simulator/view/gui/ControlPanel.hpp"
#include "source/simulator/view/gui/ParametersPanel.hpp"
#include "source/simulator/view/gui/ScopesPanel.hpp"
#include <QMainWindow>

namespace simulator
{
    class Gui
        : public QMainWindow
        , public ThreePhaseMotorModelObserver
    {
        Q_OBJECT

    public:
        Gui(ThreePhaseMotorModel& model, foc::Runner& runner, const ThreePhaseMotorModel::Parameters& motorParameters, const ParametersPanel::PidParameters& pidParameters, QWidget* parent = nullptr);

        // ThreePhaseMotorModelObserver
        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta) override;
        void Finished() override;

    signals:
        void speedChanged(int rpm);

    private:
        foc::Runner& runner;

        ControlPanel* controlPanel;
        ParametersPanel* parametersPanel;
        ScopesPanel* scopesPanel;

        foc::Radians previousTheta{ 0.0f };
        bool hasPreviousTheta = false;
    };
}
