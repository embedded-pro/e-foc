#pragma once

#include <QGroupBox>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

namespace simulator
{
    class Gui : public QMainWindow
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

        QWidget& CurrentChartPlaceholder();
        QWidget& PositionSpeedChartPlaceholder();

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

        QWidget* currentChartPlaceholder;
        QWidget* positionSpeedChartPlaceholder;
    };
}
