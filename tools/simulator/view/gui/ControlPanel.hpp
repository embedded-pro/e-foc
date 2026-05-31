#pragma once

#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QString>
#include <QWidget>

namespace simulator
{
    class ControlPanel
        : public QWidget
    {
        Q_OBJECT

    public:
        struct SetpointConfig
        {
            QString label;
            QString unit;
            int min;
            int max;
            int tickInterval;
            int defaultValue;
        };

        // Operating mode of the control panel. Determines which controls are enabled
        // and protects calibration cycles from being interrupted by Stop.
        // - Idle:        Calibration + Start enabled; Stop disabled. Default.
        // - Running:     Calibration + Start disabled; Stop enabled.
        // - Calibrating: All controls disabled. Stop cannot interrupt calibration.
        enum class Mode
        {
            Idle,
            Running,
            Calibrating
        };

        explicit ControlPanel(const SetpointConfig& config, QWidget* parent = nullptr);

        void SetMode(Mode mode);
        void SetStatus(const QString& status);
        void DisableMechanicalIdent();
        void DisableElectricalIdent();
        void DisableAlignment();
        void CalibrationFinished();

    signals:
        void startClicked();
        void stopClicked();
        void setpointChanged(int value);
        void loadChanged(double torqueNm);
        void alignClicked();
        void identifyElectricalClicked();
        void identifyMechanicalClicked();
        void statusChanged(const QString& status);

    private:
        void Build(const SetpointConfig& config);

        QPushButton* alignButton;
        QPushButton* identifyElectricalButton;
        QPushButton* identifyMechanicalButton;
        QPushButton* startButton;
        QPushButton* stopButton;
        QSlider* setpointSlider;
        QLabel* setpointValueLabel;
        QDoubleSpinBox* loadSpinBox;
        QLabel* statusLabel;
        QString setpointUnit;
        Mode mode{ Mode::Idle };
        bool alignmentAvailable{ true };
        bool electricalIdentAvailable{ true };
        bool mechanicalIdentAvailable{ true };
    };
}
