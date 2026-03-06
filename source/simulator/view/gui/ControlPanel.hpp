#pragma once

#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QWidget>
#include <QString>

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

    signals:
        void startClicked();
        void stopClicked();
        void setpointChanged(int value);
        void loadChanged(double torqueNm);

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
