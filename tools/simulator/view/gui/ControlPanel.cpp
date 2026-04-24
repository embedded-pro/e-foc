#include "tools/simulator/view/gui/ControlPanel.hpp"
#include <QDoubleSpinBox>
#include <QFont>
#include <QGroupBox>
#include <QString>
#include <QVBoxLayout>
#include <memory>

namespace simulator
{
    namespace
    {
        template<typename T, typename... Args>
        T* QtOwned(Args&&... args)
        {
            return std::make_unique<T>(std::forward<Args>(args)...).release();
        }
    }

    ControlPanel::ControlPanel(const SetpointConfig& config, QWidget* parent)
        : QWidget(parent)
    {
        Build(config);
    }

    void ControlPanel::Build(const SetpointConfig& config)
    {
        setpointUnit = config.unit;

        auto* layout = QtOwned<QVBoxLayout>(this);

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

        auto* setpointLabel = QtOwned<QLabel>(config.label, this);
        controlLayout->addWidget(setpointLabel);

        setpointValueLabel = QtOwned<QLabel>(QString("%1 %2").arg(config.defaultValue).arg(config.unit), this);
        setpointValueLabel->setAlignment(Qt::AlignCenter);
        QFont boldFont;
        boldFont.setBold(true);
        setpointValueLabel->setFont(boldFont);
        controlLayout->addWidget(setpointValueLabel);

        setpointSlider = QtOwned<QSlider>(Qt::Horizontal, this);
        setpointSlider->setMinimum(config.min);
        setpointSlider->setMaximum(config.max);
        setpointSlider->setValue(config.defaultValue);
        setpointSlider->setTickPosition(QSlider::TicksBelow);
        setpointSlider->setTickInterval(config.tickInterval);
        controlLayout->addWidget(setpointSlider);

        controlGroup->setLayout(controlLayout);
        layout->addWidget(controlGroup);

        // Load group
        auto* loadGroup = QtOwned<QGroupBox>("Load", this);
        auto* loadLayout = QtOwned<QVBoxLayout>();

        auto* loadLabel = QtOwned<QLabel>("Torque (N\xC2\xB7m):", this);
        loadLayout->addWidget(loadLabel);

        loadSpinBox = QtOwned<QDoubleSpinBox>(this);
        loadSpinBox->setRange(0.0, 1.0);
        loadSpinBox->setSingleStep(0.001);
        loadSpinBox->setDecimals(4);
        loadSpinBox->setValue(0.02);
        loadSpinBox->setSuffix(" N\xC2\xB7m");
        loadLayout->addWidget(loadSpinBox);

        loadGroup->setLayout(loadLayout);
        layout->addWidget(loadGroup);

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
                SetMode(Mode::Running);
                emit startClicked();
            });

        connect(stopButton, &QPushButton::clicked, this, [this]()
            {
                if (mode != Mode::Running)
                    return;
                SetMode(Mode::Idle);
                emit stopClicked();
            });

        connect(setpointSlider, &QSlider::valueChanged, this, [this](int value)
            {
                setpointValueLabel->setText(QString("%1 %2").arg(value).arg(setpointUnit));
                emit setpointChanged(value);
            });

        connect(loadSpinBox, &QDoubleSpinBox::valueChanged, this, [this](double value)
            {
                emit loadChanged(value);
            });

        connect(alignButton, &QPushButton::clicked, this, [this]()
            {
                SetMode(Mode::Calibrating);
                statusLabel->setText("Aligning...");
                emit statusChanged(statusLabel->text());
                emit alignClicked();
            });

        connect(identifyElectricalButton, &QPushButton::clicked, this, [this]()
            {
                SetMode(Mode::Calibrating);
                statusLabel->setText("Identifying electrical...");
                emit statusChanged(statusLabel->text());
                emit identifyElectricalClicked();
            });

        connect(identifyMechanicalButton, &QPushButton::clicked, this, [this]()
            {
                SetMode(Mode::Calibrating);
                statusLabel->setText("Identifying mechanical...");
                emit statusChanged(statusLabel->text());
                emit identifyMechanicalClicked();
            });

        SetMode(Mode::Idle);
    }

    void ControlPanel::SetMode(Mode newMode)
    {
        mode = newMode;
        switch (mode)
        {
            case Mode::Idle:
                alignButton->setEnabled(alignmentAvailable);
                identifyElectricalButton->setEnabled(electricalIdentAvailable);
                identifyMechanicalButton->setEnabled(mechanicalIdentAvailable);
                startButton->setEnabled(true);
                stopButton->setEnabled(false);
                statusLabel->setText("Idle");
                emit statusChanged(statusLabel->text());
                break;

            case Mode::Running:
                alignButton->setEnabled(false);
                identifyElectricalButton->setEnabled(false);
                identifyMechanicalButton->setEnabled(false);
                startButton->setEnabled(false);
                stopButton->setEnabled(true);
                statusLabel->setText("Running");
                emit statusChanged(statusLabel->text());
                break;

            case Mode::Calibrating:
                alignButton->setEnabled(false);
                identifyElectricalButton->setEnabled(false);
                identifyMechanicalButton->setEnabled(false);
                startButton->setEnabled(false);
                stopButton->setEnabled(false);
                // Status text is set by the caller before SetMode.
                break;
        }
    }

    void ControlPanel::SetStatus(const QString& status)
    {
        statusLabel->setText(status);
        emit statusChanged(status);
    }

    void ControlPanel::DisableMechanicalIdent()
    {
        mechanicalIdentAvailable = false;
        identifyMechanicalButton->setEnabled(false);
        identifyMechanicalButton->setToolTip("Mechanical identification not available in this control mode");
    }

    void ControlPanel::DisableElectricalIdent()
    {
        electricalIdentAvailable = false;
        identifyElectricalButton->setEnabled(false);
    }

    void ControlPanel::DisableAlignment()
    {
        alignmentAvailable = false;
        alignButton->setEnabled(false);
    }

    void ControlPanel::CalibrationFinished()
    {
        SetMode(Mode::Idle);
    }
}
