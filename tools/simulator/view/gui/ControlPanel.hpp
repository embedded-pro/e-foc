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

        explicit ControlPanel(const SetpointConfig& config, QWidget* parent = nullptr);

        void SetEditable(bool editable);
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
    };
}
