#include "tools/can_commander/gui/SetpointKnob.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include "ui/theme/Theme.hpp"
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace tool
{
    namespace
    {
        using ui::theme::ColorRole;

        // QDial spreads a non-wrapping range over 300 degrees, clockwise from the lower left; drawing
        // on the same arc keeps the pointer under the mouse while dragging.
        constexpr double startAngle{ 240.0 };
        constexpr double sweep{ 300.0 };

        QColor ThemeColor(ColorRole role)
        {
            return ui::backend::qt::ToQt(ui::theme::Current().Get(role));
        }

        QPointF OnCircle(QPointF centre, double radius, double degrees)
        {
            const auto radians = degrees * std::numbers::pi / 180.0;
            return centre + QPointF{ radius * std::cos(radians), -radius * std::sin(radians) };
        }

        int Sixteenths(double degrees)
        {
            return static_cast<int>(std::lround(degrees * 16.0));
        }
    }

    QString FormatSetpoint(const SetpointAxis& axis, int32_t counts)
    {
        return QString::number(axis.DisplayOf(counts), 'f', axis.Decimals()) + ui::backend::qt::ToQt(axis.Suffix());
    }

    SetpointKnob::SetpointKnob(const SetpointAxis& axis, QWidget* parent)
        : QDial(parent)
        , axis(axis)
    {
        setRange(-axis.MaxCounts(), axis.MaxCounts());
        setSingleStep(1);
        setPageStep(std::max(1, axis.MaxCounts() / 20));
        setWrapping(false);
        setNotchesVisible(false);
        setFocusPolicy(Qt::StrongFocus);
        setValue(0);
    }

    void SetpointKnob::SetAccepted(std::optional<int32_t> counts)
    {
        accepted = counts;
        update();
    }

    QString SetpointKnob::Format(int32_t counts) const
    {
        return FormatSetpoint(axis, counts);
    }

    QSize SetpointKnob::sizeHint() const
    {
        return { 170, 170 };
    }

    QSize SetpointKnob::minimumSizeHint() const
    {
        return { 120, 120 };
    }

    double SetpointKnob::AngleOf(int32_t counts) const
    {
        const auto fraction = static_cast<double>(counts - minimum()) / std::max(1, maximum() - minimum());
        return startAngle - sweep * fraction;
    }

    void SetpointKnob::paintEvent(QPaintEvent*)
    {
        QPainter painter{ this };
        painter.setRenderHint(QPainter::Antialiasing);

        const auto side = std::min(width(), height()) - 6.0;
        const auto track = std::max(5.0, side / 14.0);
        const QRectF arcRect{ (width() - side + track) / 2.0, (height() - side + track) / 2.0, side - track, side - track };
        const auto centre = arcRect.center();
        const auto radius = arcRect.width() / 2.0;
        const auto valueColor = ThemeColor(isEnabled() ? ColorRole::Accent : ColorRole::Neutral);

        painter.setPen(QPen{ ThemeColor(ColorRole::GridMinor), track, Qt::SolidLine, Qt::RoundCap });
        painter.drawArc(arcRect, Sixteenths(startAngle), Sixteenths(-sweep));

        const auto zero = AngleOf(0);
        const auto angle = AngleOf(value());
        painter.setPen(QPen{ valueColor, track, Qt::SolidLine, Qt::RoundCap });
        painter.drawArc(arcRect, Sixteenths(zero), Sixteenths(angle - zero));

        if (accepted.has_value())
        {
            const auto acceptedAngle = AngleOf(*accepted);
            painter.setPen(QPen{ ThemeColor(ColorRole::Text), 2.0, Qt::SolidLine, Qt::RoundCap });
            painter.drawLine(OnCircle(centre, radius - track, acceptedAngle), OnCircle(centre, radius + track * 0.8, acceptedAngle));
        }

        const auto body = radius - track * 1.6;
        painter.setPen(QPen{ ThemeColor(hasFocus() ? ColorRole::Accent : ColorRole::GridMajor), hasFocus() ? 2.0 : 1.0 });
        painter.setBrush(ThemeColor(ColorRole::Surface));
        painter.drawEllipse(centre, body, body);

        painter.setPen(QPen{ valueColor, 3.0, Qt::SolidLine, Qt::RoundCap });
        painter.drawLine(OnCircle(centre, body * 0.72, angle), OnCircle(centre, body * 0.92, angle));

        painter.setPen(ThemeColor(isEnabled() ? ColorRole::Text : ColorRole::TextMuted));
        painter.drawText(QRectF{ centre.x() - body * 0.68, centre.y() - body * 0.4, body * 1.36, body * 0.8 }, Qt::AlignCenter, Format(value()));
    }
}
