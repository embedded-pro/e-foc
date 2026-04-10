#pragma once

#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "tools/simulator/model/Model.hpp"
#include "tools/simulator/view/gui/ControlPanel.hpp"
#include "tools/simulator/view/gui/ParametersPanel.hpp"
#include "tools/simulator/view/gui/ScopesPanel.hpp"
#include <QMainWindow>

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
            const ControlPanel::SetpointConfig& setpointConfig, QWidget* parent = nullptr);

        // ThreePhaseMotorModelObserver
        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta) override;
        void Finished() override;

    signals:
        void setpointChanged(int value);

    public:
        void UpdatePidParameters(const ParametersPanel::PidParameters& pidParameters);

    private:
        ThreePhaseMotorModel& model;
        foc::Controllable& controller;
        infra::EventDispatcherWithWeakPtr& eventDispatcher;

        ControlPanel* controlPanel;
        ParametersPanel* parametersPanel;
        ScopesPanel* scopesPanel;

        foc::Radians previousTheta{ 0.0f };
        bool hasPreviousTheta = false;
    };
}
