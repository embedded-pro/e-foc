#include "source/simulator/view/gui/Gui.hpp"
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <memory>

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

        // Qt's parent-child mechanism manages widget lifetime;
        // this helper avoids raw 'new' while transferring ownership to the parent.
        template<typename T, typename... Args>
        T* QtOwned(Args&&... args)
        {
            return std::make_unique<T>(std::forward<Args>(args)...).release();
        }
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

        auto* centralWidget = QtOwned<QWidget>(this);
        setCentralWidget(centralWidget);

        auto* mainLayout = QtOwned<QHBoxLayout>(centralWidget);
        mainLayout->addWidget(CreateLeftPanel(), leftPanelStretch);
        mainLayout->addWidget(CreateMiddlePanel(), middlePanelStretch);
        mainLayout->addWidget(CreateRightPanel(), rightPanelStretch);
    }

    QWidget* Gui::CreateLeftPanel()
    {
        auto* panel = QtOwned<QWidget>(this);
        auto* layout = QtOwned<QVBoxLayout>(panel);

        // Calibration group
        auto* calibrationGroup = QtOwned<QGroupBox>("Calibration", this);
        auto* calibrationLayout = QtOwned<QVBoxLayout>();

        alignButton = QtOwned<QPushButton>("Align Motor", this);
        calibrationLayout->addWidget(alignButton);

        identifyElectricalButton = QtOwned<QPushButton>("Identify Electrical", this);
        calibrationLayout->addWidget(identifyElectricalButton);

        identifyMechanicalButton = QtOwned<QPushButton>("Identify Mechanical", this);
        calibrationLayout->addWidget(identifyMechanicalButton);

        calibrationGroup->setLayout(calibrationLayout);
        layout->addWidget(calibrationGroup);

        // Motor control group
        auto* controlGroup = QtOwned<QGroupBox>("Motor Control", this);
        auto* controlLayout = QtOwned<QVBoxLayout>();

        startButton = QtOwned<QPushButton>("Start", this);
        controlLayout->addWidget(startButton);

        stopButton = QtOwned<QPushButton>("Stop", this);
        stopButton->setEnabled(false);
        controlLayout->addWidget(stopButton);

        auto* speedLabel = QtOwned<QLabel>("Speed Setpoint:", this);
        controlLayout->addWidget(speedLabel);

        speedValueLabel = QtOwned<QLabel>("0 RPM", this);
        speedValueLabel->setAlignment(Qt::AlignCenter);
        QFont boldFont;
        boldFont.setBold(true);
        speedValueLabel->setFont(boldFont);
        controlLayout->addWidget(speedValueLabel);

        speedSlider = QtOwned<QSlider>(Qt::Horizontal, this);
        speedSlider->setMinimum(speedSliderMin);
        speedSlider->setMaximum(speedSliderMax);
        speedSlider->setValue(0);
        speedSlider->setTickPosition(QSlider::TicksBelow);
        speedSlider->setTickInterval(speedSliderTickInterval);
        controlLayout->addWidget(speedSlider);

        controlGroup->setLayout(controlLayout);
        layout->addWidget(controlGroup);

        // Status group
        auto* statusGroup = QtOwned<QGroupBox>("Status", this);
        auto* statusLayout = QtOwned<QVBoxLayout>();

        statusLabel = QtOwned<QLabel>("Idle", this);
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLayout->addWidget(statusLabel);

        statusGroup->setLayout(statusLayout);
        layout->addWidget(statusGroup);

        layout->addStretch();
        return panel;
    }

    QWidget* Gui::CreateMiddlePanel()
    {
        auto* panel = QtOwned<QWidget>(this);
        auto* layout = QtOwned<QVBoxLayout>(panel);

        // Electrical parameters
        auto* electricalGroup = QtOwned<QGroupBox>("Electrical Parameters", this);
        auto* electricalLayout = QtOwned<QGridLayout>();

        electricalLayout->addWidget(QtOwned<QLabel>("Resistance (R):", this), 0, 0);
        resistanceLabel = QtOwned<QLabel>("---", this);
        resistanceLabel->setAlignment(Qt::AlignRight);
        electricalLayout->addWidget(resistanceLabel, 0, 1);

        electricalLayout->addWidget(QtOwned<QLabel>("Inductance (L):", this), 1, 0);
        inductanceLabel = QtOwned<QLabel>("---", this);
        inductanceLabel->setAlignment(Qt::AlignRight);
        electricalLayout->addWidget(inductanceLabel, 1, 1);

        electricalGroup->setLayout(electricalLayout);
        layout->addWidget(electricalGroup);

        // Mechanical parameters
        auto* mechanicalGroup = QtOwned<QGroupBox>("Mechanical Parameters", this);
        auto* mechanicalLayout = QtOwned<QGridLayout>();

        mechanicalLayout->addWidget(QtOwned<QLabel>("Friction (B):", this), 0, 0);
        frictionLabel = QtOwned<QLabel>("---", this);
        frictionLabel->setAlignment(Qt::AlignRight);
        mechanicalLayout->addWidget(frictionLabel, 0, 1);

        mechanicalLayout->addWidget(QtOwned<QLabel>("Inertia (J):", this), 1, 0);
        inertiaLabel = QtOwned<QLabel>("---", this);
        inertiaLabel->setAlignment(Qt::AlignRight);
        mechanicalLayout->addWidget(inertiaLabel, 1, 1);

        mechanicalGroup->setLayout(mechanicalLayout);
        layout->addWidget(mechanicalGroup);

        // Current controller gains
        auto* currentPidGroup = QtOwned<QGroupBox>("Current Controller (PI)", this);
        auto* currentPidLayout = QtOwned<QGridLayout>();

        currentPidLayout->addWidget(QtOwned<QLabel>("Kp:", this), 0, 0);
        currentKpLabel = QtOwned<QLabel>("---", this);
        currentKpLabel->setAlignment(Qt::AlignRight);
        currentPidLayout->addWidget(currentKpLabel, 0, 1);

        currentPidLayout->addWidget(QtOwned<QLabel>("Ki:", this), 1, 0);
        currentKiLabel = QtOwned<QLabel>("---", this);
        currentKiLabel->setAlignment(Qt::AlignRight);
        currentPidLayout->addWidget(currentKiLabel, 1, 1);

        currentPidGroup->setLayout(currentPidLayout);
        layout->addWidget(currentPidGroup);

        // Speed controller gains
        auto* speedPidGroup = QtOwned<QGroupBox>("Speed Controller (PID)", this);
        auto* speedPidLayout = QtOwned<QGridLayout>();

        speedPidLayout->addWidget(QtOwned<QLabel>("Kp:", this), 0, 0);
        speedKpLabel = QtOwned<QLabel>("---", this);
        speedKpLabel->setAlignment(Qt::AlignRight);
        speedPidLayout->addWidget(speedKpLabel, 0, 1);

        speedPidLayout->addWidget(QtOwned<QLabel>("Ki:", this), 1, 0);
        speedKiLabel = QtOwned<QLabel>("---", this);
        speedKiLabel->setAlignment(Qt::AlignRight);
        speedPidLayout->addWidget(speedKiLabel, 1, 1);

        speedPidLayout->addWidget(QtOwned<QLabel>("Kd:", this), 2, 0);
        speedKdLabel = QtOwned<QLabel>("---", this);
        speedKdLabel->setAlignment(Qt::AlignRight);
        speedPidLayout->addWidget(speedKdLabel, 2, 1);

        speedPidGroup->setLayout(speedPidLayout);
        layout->addWidget(speedPidGroup);

        layout->addStretch();
        return panel;
    }

    QWidget* Gui::CreateRightPanel()
    {
        auto* panel = QtOwned<QWidget>(this);
        auto* layout = QtOwned<QVBoxLayout>(panel);

        // Phase currents chart placeholder
        auto* currentGroup = QtOwned<QGroupBox>("Phase Currents (A, B, C)", this);
        auto* currentLayout = QtOwned<QVBoxLayout>();

        currentChartPlaceholder = QtOwned<QWidget>(this);
        currentChartPlaceholder->setMinimumHeight(300);
        currentChartPlaceholder->setStyleSheet("background-color: #1e1e1e;");
        currentLayout->addWidget(currentChartPlaceholder);

        currentGroup->setLayout(currentLayout);
        layout->addWidget(currentGroup);

        // Position & Speed chart placeholder
        auto* positionSpeedGroup = QtOwned<QGroupBox>("Position && Speed", this);
        auto* positionSpeedLayout = QtOwned<QVBoxLayout>();

        positionSpeedChartPlaceholder = QtOwned<QWidget>(this);
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
