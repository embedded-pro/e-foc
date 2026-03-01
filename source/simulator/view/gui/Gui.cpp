#include "source/simulator/view/gui/Gui.hpp"
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QString>
#include <QVBoxLayout>
#include <array>
#include <cmath>
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

    Gui::Gui(ThreePhaseMotorModel& model, const ThreePhaseMotorModel::Parameters& motorParameters, const PidParameters& pidParameters, QWidget* parent)
        : QMainWindow(parent)
        , ThreePhaseMotorModelObserver(model)
        , motorParameters(motorParameters)
        , pidParameters(pidParameters)
    {
        SetupUi();
        PopulateMotorParameters();
        PopulatePidParameters();
    }

    void Gui::Started()
    {
        statusLabel->setText("Running");
        hasPreviousTheta = false;
    }

    void Gui::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta)
    {
        const std::array<float, 3> currents = { currentPhases.a.Value(), currentPhases.b.Value(), currentPhases.c.Value() };
        currentScope->AddSample(currents);

        float omega = 0.0f;
        if (hasPreviousTheta)
            omega = theta.Value() - previousTheta.Value();

        previousTheta = theta;
        hasPreviousTheta = true;

        const std::array<float, 2> positionSpeed = { theta.Value(), omega };
        positionSpeedScope->AddSample(positionSpeed);
    }

    void Gui::Finished()
    {
        statusLabel->setText("Finished");
    }

    void Gui::SetupUi()
    {
        setWindowTitle("e-foc Simulator");
        setFixedSize(windowWidth, windowHeight);

        auto* centralWidget = QtOwned<QWidget>(this);
        setCentralWidget(centralWidget);

        auto* mainLayout = QtOwned<QHBoxLayout>(centralWidget);
        mainLayout->addWidget(CreateLeftPanel(), leftPanelStretch);
        mainLayout->addWidget(CreateMiddlePanel(), middlePanelStretch);
        mainLayout->addWidget(CreateRightPanel(), rightPanelStretch);
    }

    void Gui::SetEditable(bool editable)
    {
        alignButton->setEnabled(editable);
        identifyElectricalButton->setEnabled(editable);
        identifyMechanicalButton->setEnabled(editable);
        speedSlider->setEnabled(editable);
        startButton->setEnabled(editable);
        stopButton->setEnabled(!editable);
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

        connect(startButton, &QPushButton::clicked, this, [this]()
            {
                SetEditable(false);
            });

        connect(stopButton, &QPushButton::clicked, this, [this]()
            {
                SetEditable(true);
            });

        connect(speedSlider, &QSlider::valueChanged, this, [this](int value)
            {
                speedValueLabel->setText(QString("%1 RPM").arg(value));
            });

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

        // Phase currents scope
        auto* currentGroup = QtOwned<QGroupBox>("Phase Currents (A, B, C)", this);
        auto* currentLayout = QtOwned<QVBoxLayout>();

        currentScope = QtOwned<ScopeWidget>(this);
        currentScope->SetChannelCount(3);
        currentScope->SetChannelConfig(0, { "Ia", QColor(0, 150, 255) });
        currentScope->SetChannelConfig(1, { "Ib", QColor(255, 165, 0) });
        currentScope->SetChannelConfig(2, { "Ic", QColor(0, 200, 80) });

        currentScopeToolbar = QtOwned<ScopeToolbar>(*currentScope, this);

        currentLayout->addWidget(currentScopeToolbar);
        currentLayout->addWidget(currentScope);

        currentGroup->setLayout(currentLayout);
        layout->addWidget(currentGroup);

        // Position & Speed scope
        auto* positionSpeedGroup = QtOwned<QGroupBox>("Position && Speed", this);
        auto* positionSpeedLayout = QtOwned<QVBoxLayout>();

        positionSpeedScope = QtOwned<ScopeWidget>(this);
        positionSpeedScope->SetChannelCount(2);
        positionSpeedScope->SetChannelConfig(0, { "θ (rad)", QColor(0, 150, 255) });
        positionSpeedScope->SetChannelConfig(1, { "ω (rad/s)", QColor(255, 100, 100) });

        positionSpeedScopeToolbar = QtOwned<ScopeToolbar>(*positionSpeedScope, this);

        positionSpeedLayout->addWidget(positionSpeedScopeToolbar);
        positionSpeedLayout->addWidget(positionSpeedScope);

        positionSpeedGroup->setLayout(positionSpeedLayout);
        layout->addWidget(positionSpeedGroup);

        return panel;
    }

    void Gui::PopulateMotorParameters()
    {
        resistanceLabel->setText(QString::number(static_cast<double>(motorParameters.R.Value()), 'g', 4) + " Ω");
        inductanceLabel->setText(QString::number(static_cast<double>(motorParameters.Ld.Value()), 'g', 4) + " H");
        frictionLabel->setText(QString::number(static_cast<double>(motorParameters.B.Value()), 'g', 4) + " N·m·s/rad");
        inertiaLabel->setText(QString::number(static_cast<double>(motorParameters.J.Value()), 'g', 4) + " kg·m²");
    }

    void Gui::PopulatePidParameters()
    {
        currentKpLabel->setText(QString::number(static_cast<double>(pidParameters.currentKp), 'g', 6));
        currentKiLabel->setText(QString::number(static_cast<double>(pidParameters.currentKi), 'g', 6));
        speedKpLabel->setText(QString::number(static_cast<double>(pidParameters.speedKp), 'g', 6));
        speedKiLabel->setText(QString::number(static_cast<double>(pidParameters.speedKi), 'g', 6));
        speedKdLabel->setText(QString::number(static_cast<double>(pidParameters.speedKd), 'g', 6));
    }
}
