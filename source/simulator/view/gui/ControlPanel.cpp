#include "source/simulator/view/gui/ControlPanel.hpp"
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
        constexpr int speedSliderMin = -3000;
        constexpr int speedSliderMax = 3000;
        constexpr int speedSliderTickInterval = 500;

        template<typename T, typename... Args>
        T* QtOwned(Args&&... args)
        {
            return std::make_unique<T>(std::forward<Args>(args)...).release();
        }
    }

    ControlPanel::ControlPanel(QWidget* parent)
        : QWidget(parent)
    {
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
                SetEditable(false);
                emit startClicked();
            });

        connect(stopButton, &QPushButton::clicked, this, [this]()
            {
                SetEditable(true);
                emit stopClicked();
            });

        connect(speedSlider, &QSlider::valueChanged, this, [this](int value)
            {
                speedValueLabel->setText(QString("%1 RPM").arg(value));
                emit speedChanged(value);
            });

        connect(loadSpinBox, &QDoubleSpinBox::valueChanged, this, [this](double value)
            {
                emit loadChanged(value);
            });
    }

    void ControlPanel::SetEditable(bool editable)
    {
        alignButton->setEnabled(editable);
        identifyElectricalButton->setEnabled(editable);
        identifyMechanicalButton->setEnabled(editable);
        startButton->setEnabled(editable);
        stopButton->setEnabled(!editable);
    }

    void ControlPanel::SetStatus(const QString& status)
    {
        statusLabel->setText(status);
    }
}
