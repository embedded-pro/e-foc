#pragma once

#include "tools/virtual_motor/app/logic/Rig.hpp"
#include <QWidget>
#include <array>
#include <optional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace virtual_motor::app
{
    class CaptureBar
        : public QWidget
    {
        Q_OBJECT

    public:
        CaptureBar(RigMotor& motor, QWidget* parent = nullptr);

        void Refresh();
        port::CaptureRequest Request() const;
        std::optional<uint8_t> ArmedSignal() const;

    private:
        void BuildFields();
        void ConnectFields();
        void KindChanged();
        void SignalChanged();
        void Arm();
        bool UsesSignal() const;
        bool UsesLeds() const;
        uint8_t LedMask() const;

        RigMotor& motor;
        QComboBox* kind;
        QComboBox* signal;
        QDoubleSpinBox* level;
        std::array<QCheckBox*, 3> leds{};
        QSpinBox* preTrigger;
        QSpinBox* length;
        QPushButton* arm;
        QPushButton* cancel;
        QLabel* status;
        std::optional<uint8_t> armedSignal;
    };
}
