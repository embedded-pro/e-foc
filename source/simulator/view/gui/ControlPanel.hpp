#pragma once

#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

namespace simulator
{
    class ControlPanel
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit ControlPanel(QWidget* parent = nullptr);

        void SetEditable(bool editable);
        void SetStatus(const QString& status);

    signals:
        void startClicked();
        void stopClicked();
        void speedChanged(int rpm);
        void loadChanged(double torqueNm);

    private:
        QPushButton* alignButton;
        QPushButton* identifyElectricalButton;
        QPushButton* identifyMechanicalButton;
        QPushButton* startButton;
        QPushButton* stopButton;
        QSlider* speedSlider;
        QLabel* speedValueLabel;
        QDoubleSpinBox* loadSpinBox;
        QLabel* statusLabel;
    };
}
