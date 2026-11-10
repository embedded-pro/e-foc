#pragma once

#include "tools/virtual_motor/app/logic/Rig.hpp"
#include <QWidget>

class QLabel;
class QPushButton;

namespace virtual_motor::app
{
    class LogView;
    class TorqueNoisePanel;

    class MotorWindow
        : public QWidget
    {
        Q_OBJECT

    public:
        MotorWindow(Rig& rig, uint8_t node, QWidget* parent = nullptr);

        void Refresh();
        void AppendLog(const QString& line);
        void ShowRefusal(const port::Refused& refused);

    private:
        QWidget* BuildHeader();
        QWidget* BuildControls();
        void ConnectControls();
        void RefreshStatus(const RigMotor& motor);
        void RefreshControls(const RigMotor& motor);

        Rig& rig;
        uint8_t node;
        QLabel* state;
        QLabel* time;
        QLabel* realTime;
        QLabel* leds;
        QLabel* protection;
        QLabel* disturbances;
        QPushButton* pause;
        QPushButton* step;
        QPushButton* clearAll;
        QPushButton* powerOn;
        QPushButton* powerCycle;
        QPushButton* powerOff;
        TorqueNoisePanel* panel;
        LogView* log;
    };
}
