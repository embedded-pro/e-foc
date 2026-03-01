#include "source/simulator/view/gui/Gui.hpp"
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace simulator
{
    namespace
    {
        constexpr int windowWidth = 1400;
        constexpr int windowHeight = 800;
        constexpr int speedSliderMin = -3000;
        constexpr int speedSliderMax = 3000;
        constexpr int speedSliderTickInterval = 500;
        constexpr int leftPanelStretch = 1;
        constexpr int middlePanelStretch = 2;
        constexpr int rightPanelStretch = 3;
    }

    Gui::Gui(QWidget* parent)
        : QMainWindow(parent)
    {
        SetupUi();
    }

    void Gui::SetupUi()
    {
        setWindowTitle("FOC Motor Controller Simulator");
        setFixedSize(windowWidth, windowHeight);

        auto* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        auto* mainLayout = new QHBoxLayout(centralWidget);
        mainLayout->addWidget(CreateLeftPanel(), leftPanelStretch);
        mainLayout->addWidget(CreateMiddlePanel(), middlePanelStretch);
        mainLayout->addWidget(CreateRightPanel(), rightPanelStretch);
    }

    QWidget* Gui::CreateLeftPanel()
    {
        auto* panel = new QWidget(this);
        auto* layout = new QVBoxLayout(panel);

        // Calibration group
        auto* calibrationGroup = new QGroupBox("Calibration", this);
        auto* calibrationLayout = new QVBoxLayout();

        alignButton = new QPushButton("Align Motor", this);
        calibrationLayout->addWidget(alignButton);

        identifyElectricalButton = new QPushButton("Identify Electrical", this);
        calibrationLayout->addWidget(identifyElectricalButton);

        identifyMechanicalButton = new QPushButton("Identify Mechanical", this);
        calibrationLayout->addWidget(identifyMechanicalButton);

        calibrationGroup->setLayout(calibrationLayout);
        layout->addWidget(calibrationGroup);

        // Motor control group
        auto* controlGroup = new QGroupBox("Motor Control", this);
        auto* controlLayout = new QVBoxLayout();

        startButton = new QPushButton("Start", this);
        controlLayout->addWidget(startButton);

        stopButton = new QPushButton("Stop", this);
        stopButton->setEnabled(false);
        controlLayout->addWidget(stopButton);

        auto* speedLabel = new QLabel("Speed Setpoint:", this);
        controlLayout->addWidget(speedLabel);

        speedValueLabel = new QLabel("0 RPM", this);
        speedValueLabel->setAlignment(Qt::AlignCenter);
        QFont boldFont;
        boldFont.setBold(true);
        speedValueLabel->setFont(boldFont);
        controlLayout->addWidget(speedValueLabel);

        speedSlider = new QSlider(Qt::Horizontal, this);
        speedSlider->setMinimum(speedSliderMin);
        speedSlider->setMaximum(speedSliderMax);
        speedSlider->setValue(0);
        speedSlider->setTickPosition(QSlider::TicksBelow);
        speedSlider->setTickInterval(speedSliderTickInterval);
        controlLayout->addWidget(speedSlider);

        controlGroup->setLayout(controlLayout);
        layout->addWidget(controlGroup);

        // Status group
        auto* statusGroup = new QGroupBox("Status", this);
        auto* statusLayout = new QVBoxLayout();

        statusLabel = new QLabel("Idle", this);
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLayout->addWidget(statusLabel);

        statusGroup->setLayout(statusLayout);
        layout->addWidget(statusGroup);

        layout->addStretch();
        return panel;
    }

    QWidget* Gui::CreateMiddlePanel()
    {
        auto* panel = new QWidget(this);
        auto* layout = new QVBoxLayout(panel);

        // Electrical parameters
        auto* electricalGroup = new QGroupBox("Electrical Parameters", this);
        auto* electricalLayout = new QGridLayout();

        electricalLayout->addWidget(new QLabel("Resistance (R):", this), 0, 0);
        resistanceLabel = new QLabel("---", this);
        resistanceLabel->setAlignment(Qt::AlignRight);
        electricalLayout->addWidget(resistanceLabel, 0, 1);

        electricalLayout->addWidget(new QLabel("Inductance (L):", this), 1, 0);
        inductanceLabel = new QLabel("---", this);
        inductanceLabel->setAlignment(Qt::AlignRight);
        electricalLayout->addWidget(inductanceLabel, 1, 1);

        electricalGroup->setLayout(electricalLayout);
        layout->addWidget(electricalGroup);

        // Mechanical parameters
        auto* mechanicalGroup = new QGroupBox("Mechanical Parameters", this);
        auto* mechanicalLayout = new QGridLayout();

        mechanicalLayout->addWidget(new QLabel("Friction (B):", this), 0, 0);
        frictionLabel = new QLabel("---", this);
        frictionLabel->setAlignment(Qt::AlignRight);
        mechanicalLayout->addWidget(frictionLabel, 0, 1);

        mechanicalLayout->addWidget(new QLabel("Inertia (J):", this), 1, 0);
        inertiaLabel = new QLabel("---", this);
        inertiaLabel->setAlignment(Qt::AlignRight);
        mechanicalLayout->addWidget(inertiaLabel, 1, 1);

        mechanicalGroup->setLayout(mechanicalLayout);
        layout->addWidget(mechanicalGroup);

        // Current controller gains
        auto* currentPidGroup = new QGroupBox("Current Controller (PI)", this);
        auto* currentPidLayout = new QGridLayout();

        currentPidLayout->addWidget(new QLabel("Kp:", this), 0, 0);
        currentKpLabel = new QLabel("---", this);
        currentKpLabel->setAlignment(Qt::AlignRight);
        currentPidLayout->addWidget(currentKpLabel, 0, 1);

        currentPidLayout->addWidget(new QLabel("Ki:", this), 1, 0);
        currentKiLabel = new QLabel("---", this);
        currentKiLabel->setAlignment(Qt::AlignRight);
        currentPidLayout->addWidget(currentKiLabel, 1, 1);

        currentPidGroup->setLayout(currentPidLayout);
        layout->addWidget(currentPidGroup);

        // Speed controller gains
        auto* speedPidGroup = new QGroupBox("Speed Controller (PID)", this);
        auto* speedPidLayout = new QGridLayout();

        speedPidLayout->addWidget(new QLabel("Kp:", this), 0, 0);
        speedKpLabel = new QLabel("---", this);
        speedKpLabel->setAlignment(Qt::AlignRight);
        speedPidLayout->addWidget(speedKpLabel, 0, 1);

        speedPidLayout->addWidget(new QLabel("Ki:", this), 1, 0);
        speedKiLabel = new QLabel("---", this);
        speedKiLabel->setAlignment(Qt::AlignRight);
        speedPidLayout->addWidget(speedKiLabel, 1, 1);

        speedPidLayout->addWidget(new QLabel("Kd:", this), 2, 0);
        speedKdLabel = new QLabel("---", this);
        speedKdLabel->setAlignment(Qt::AlignRight);
        speedPidLayout->addWidget(speedKdLabel, 2, 1);

        speedPidGroup->setLayout(speedPidLayout);
        layout->addWidget(speedPidGroup);

        layout->addStretch();
        return panel;
    }

    QWidget* Gui::CreateRightPanel()
    {
        auto* panel = new QWidget(this);
        auto* layout = new QVBoxLayout(panel);

        // Phase currents chart placeholder
        auto* currentGroup = new QGroupBox("Phase Currents (A, B, C)", this);
        auto* currentLayout = new QVBoxLayout();

        currentChartPlaceholder = new QWidget(this);
        currentChartPlaceholder->setMinimumHeight(300);
        currentChartPlaceholder->setStyleSheet("background-color: #1e1e1e;");
        currentLayout->addWidget(currentChartPlaceholder);

        currentGroup->setLayout(currentLayout);
        layout->addWidget(currentGroup);

        // Position & Speed chart placeholder
        auto* positionSpeedGroup = new QGroupBox("Position && Speed", this);
        auto* positionSpeedLayout = new QVBoxLayout();

        positionSpeedChartPlaceholder = new QWidget(this);
        positionSpeedChartPlaceholder->setMinimumHeight(300);
        positionSpeedChartPlaceholder->setStyleSheet("background-color: #1e1e1e;");
        positionSpeedLayout->addWidget(positionSpeedChartPlaceholder);

        positionSpeedGroup->setLayout(positionSpeedLayout);
        layout->addWidget(positionSpeedGroup);

        return panel;
    }

    QPushButton& Gui::AlignButton()
    {
        return *alignButton;
    }

    QPushButton& Gui::IdentifyElectricalButton()
    {
        return *identifyElectricalButton;
    }

    QPushButton& Gui::IdentifyMechanicalButton()
    {
        return *identifyMechanicalButton;
    }

    QPushButton& Gui::StartButton()
    {
        return *startButton;
    }

    QPushButton& Gui::StopButton()
    {
        return *stopButton;
    }

    QSlider& Gui::SpeedSlider()
    {
        return *speedSlider;
    }

    QLabel& Gui::SpeedValueLabel()
    {
        return *speedValueLabel;
    }

    QLabel& Gui::StatusLabel()
    {
        return *statusLabel;
    }

    QLabel& Gui::ResistanceLabel()
    {
        return *resistanceLabel;
    }

    QLabel& Gui::InductanceLabel()
    {
        return *inductanceLabel;
    }

    QLabel& Gui::FrictionLabel()
    {
        return *frictionLabel;
    }

    QLabel& Gui::InertiaLabel()
    {
        return *inertiaLabel;
    }

    QLabel& Gui::CurrentKpLabel()
    {
        return *currentKpLabel;
    }

    QLabel& Gui::CurrentKiLabel()
    {
        return *currentKiLabel;
    }

    QLabel& Gui::SpeedKpLabel()
    {
        return *speedKpLabel;
    }

    QLabel& Gui::SpeedKiLabel()
    {
        return *speedKiLabel;
    }

    QLabel& Gui::SpeedKdLabel()
    {
        return *speedKdLabel;
    }

    QWidget& Gui::CurrentChartPlaceholder()
    {
        return *currentChartPlaceholder;
    }

    QWidget& Gui::PositionSpeedChartPlaceholder()
    {
        return *positionSpeedChartPlaceholder;
    }
}
