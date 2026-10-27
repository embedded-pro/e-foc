#pragma once

#include "tools/can_commander/logic/SetpointAxis.hpp"
#include <QDial>
#include <QString>
#include <optional>

namespace tool
{
    QString FormatSetpoint(const SetpointAxis& axis, int32_t counts);

    // A dial whose value is a wire count of its axis. It only chooses the value: sending it is the
    // Apply button's job, so turning the knob never moves the motor.
    class SetpointKnob
        : public QDial
    {
        Q_OBJECT

    public:
        explicit SetpointKnob(const SetpointAxis& axis, QWidget* parent = nullptr);

        void SetAccepted(std::optional<int32_t> counts);
        QString Format(int32_t counts) const;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        double AngleOf(int32_t counts) const;

        const SetpointAxis& axis;
        std::optional<int32_t> accepted;
    };
}
