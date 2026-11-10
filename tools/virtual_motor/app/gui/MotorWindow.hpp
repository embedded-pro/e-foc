#pragma once

#include "tools/virtual_motor/app/logic/Rig.hpp"
#include <QWidget>
#include <optional>

class QLabel;
class QPushButton;

namespace virtual_motor::app
{
    class CaptureBar;
    class LogView;
    class ScopeArea;
    class TorqueNoisePanel;

    class MotorWindow
        : public QWidget
    {
        Q_OBJECT

    public:
        MotorWindow(Rig& rig, uint8_t node, QWidget* parent = nullptr);
        ~MotorWindow() override;

        void Refresh();
        void RefreshCapture();
        void AppendLog(const QString& line);
        void ShowRefusal(const port::Refused& refused);

    protected:
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;
        void changeEvent(QEvent* event) override;

    private:
        QWidget* BuildHeader();
        QWidget* BuildControls();
        void ConnectControls();
        void RefreshStatus(const RigMotor& motor);
        void RefreshControls(const RigMotor& motor);
        void UpdateStreaming();

        Rig& rig;
        uint8_t node;
        QLabel* state;
        QLabel* time;
        QLabel* realTime;
        QLabel* leds;
        QLabel* protection;
        QLabel* disturbances;
        QPushButton* pause;
        QPushButton* stepPeriod;
        QPushButton* step;
        QPushButton* clearAll;
        QPushButton* powerOn;
        QPushButton* powerCycle;
        QPushButton* powerOff;
        TorqueNoisePanel* panel;
        LogView* log;
        ScopeArea* scope;
        CaptureBar* captureBar;
        std::optional<uint16_t> shownCapture;
    };
}
