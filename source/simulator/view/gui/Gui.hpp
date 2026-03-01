#pragma once

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
    {
        Q_OBJECT

    public:
        explicit Gui(QWidget* parent = nullptr);

        QPushButton& AlignButton();
        QPushButton& IdentifyElectricalButton();
        QPushButton& IdentifyMechanicalButton();
        QPushButton& StartButton();
        QPushButton& StopButton();
        QSlider& SpeedSlider();

        QLabel& SpeedValueLabel();
        QLabel& StatusLabel();

        QLabel& ResistanceLabel();
        QLabel& InductanceLabel();
        QLabel& FrictionLabel();
        QLabel& InertiaLabel();

        QLabel& CurrentKpLabel();
        QLabel& CurrentKiLabel();
        QLabel& SpeedKpLabel();
        QLabel& SpeedKiLabel();
        QLabel& SpeedKdLabel();

        ScopeWidget& CurrentScope();
        ScopeToolbar& CurrentScopeToolbar();
        ScopeWidget& PositionSpeedScope();
        ScopeToolbar& PositionSpeedScopeToolbar();

    private:
        void SetupUi();

        QWidget* CreateLeftPanel();
        QWidget* CreateMiddlePanel();
        QWidget* CreateRightPanel();

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
    };
}
