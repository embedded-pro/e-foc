#pragma once

#include "source/simulator/model/Model.hpp"
#include "source/simulator/view/gui/ScopeToolbar.hpp"
#include "source/simulator/view/gui/ScopeWidget.hpp"
#include <QGroupBox>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

namespace simulator
{
    class Gui
        : public QMainWindow
        , public ThreePhaseMotorModelObserver
    {
        Q_OBJECT

    public:
        struct PidParameters
        {
            float currentKp;
            float currentKi;
            float currentKd;
            float speedKp;
            float speedKi;
            float speedKd;
        };

        Gui(ThreePhaseMotorModel& model, const ThreePhaseMotorModel::Parameters& motorParameters, const PidParameters& pidParameters, QWidget* parent = nullptr);

        // ThreePhaseMotorModelObserver
        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta) override;
        void Finished() override;

    private:
        void SetupUi();
        void SetEditable(bool editable);

        QWidget* CreateLeftPanel();
        QWidget* CreateMiddlePanel();
        QWidget* CreateRightPanel();

        void PopulateMotorParameters();
        void PopulatePidParameters();

        const ThreePhaseMotorModel::Parameters& motorParameters;
        PidParameters pidParameters;

        QPushButton* alignButton;
        QPushButton* identifyElectricalButton;
        QPushButton* identifyMechanicalButton;
        QPushButton* startButton;
        QPushButton* stopButton;
        QSlider* speedSlider;

        QLabel* speedValueLabel;
        QLabel* statusLabel;

        QLabel* resistanceLabel;
        QLabel* inductanceLabel;
        QLabel* frictionLabel;
        QLabel* inertiaLabel;

        QLabel* currentKpLabel;
        QLabel* currentKiLabel;
        QLabel* speedKpLabel;
        QLabel* speedKiLabel;
        QLabel* speedKdLabel;

        ScopeWidget* currentScope;
        ScopeToolbar* currentScopeToolbar;
        ScopeWidget* positionSpeedScope;
        ScopeToolbar* positionSpeedScopeToolbar;

        foc::Radians previousTheta{ 0.0f };
        bool hasPreviousTheta = false;
    };
}
