#include "tools/virtual_motor/app/logic/ModulationView.hpp"
#include "numerical/math/Math.hpp"
#include "ui/theme/Theme.hpp"
#include <algorithm>
#include <format>
#include <numbers>

namespace virtual_motor::app
{
    namespace
    {
        constexpr uint8_t legVoltageA{ 9 };
        constexpr uint8_t statorVoltageAlpha{ 12 };
        constexpr uint8_t statorVoltageBeta{ 13 };
        constexpr uint8_t currentD{ 14 };
        constexpr uint8_t currentQ{ 15 };
        constexpr uint8_t busVoltage{ 22 };
        constexpr std::size_t trailSegments{ 8 };
        constexpr float minimumCurrentRange{ 0.1f };
        constexpr float margin{ 40.0f };

        ui::Rect Square(const ui::Rect& area)
        {
            const auto side = std::max(0.0f, std::min(area.width, area.height) - 2.0f * margin);
            return { area.x + (area.width - side) / 2.0f, area.y + (area.height - side) / 2.0f, side, side };
        }

        ui::Point ToScreen(ui::Point centre, float x, float y, float scale)
        {
            return { centre.x + x * scale, centre.y - y * scale };
        }
    }

    bool IsOvermodulated(float vAlpha, float vBeta, float busVolts)
    {
        return busVolts > 0.0f && math::Hypot(vAlpha, vBeta) > busVolts * std::numbers::inv_sqrt3_v<float> * 1.001f;
    }

    std::string_view ModulationView::Name() const
    {
        return "Modulation";
    }

    void ModulationView::Show(const TraceSource* newSource, TimeRange newRange, double)
    {
        source = newSource;
        range = newRange;
        RequestRepaint();
    }

    ui::Size ModulationView::MinimumSize() const
    {
        return { 640.0f, 320.0f };
    }

    void ModulationView::Paint(ui::Canvas& canvas, const ui::Rect& bounds)
    {
        canvas.FillRect(bounds, ui::theme::Current().Get(ui::theme::ColorRole::ScopeBackground));
        const auto newest = Newest();
        const auto sample = newest.has_value() ? source->Last(*newest) : port::Sample{};
        const ui::Rect left{ bounds.x, bounds.y, bounds.width / 2.0f, bounds.height };
        const ui::Rect right{ bounds.x + bounds.width / 2.0f, bounds.y, bounds.width / 2.0f, bounds.height };
        PaintHexagon(canvas, left, sample);
        PaintCurrentPlane(canvas, right, sample);
    }

    std::optional<std::size_t> ModulationView::Newest() const
    {
        if (source == nullptr || range.end <= range.start)
            return std::nullopt;
        return source->LastAtOrBefore(range.end);
    }

    std::vector<ui::Point> ModulationView::Trail(uint8_t x, uint8_t y, const Plane& plane) const
    {
        std::vector<ui::Point> trail;
        const auto newest = Newest();
        if (!newest.has_value())
            return trail;
        for (auto index = source->FirstAtOrAfter(range.start); index <= *newest; ++index)
            trail.push_back(ToScreen(plane.centre, source->Last(index)[x], source->Last(index)[y], plane.scale));
        return trail;
    }

    void ModulationView::PaintHexagon(ui::Canvas& canvas, const ui::Rect& area, const port::Sample& newest)
    {
        const auto bus = newest[busVoltage] > 0.0f ? newest[busVoltage] : 1.0f;
        hexagon.SetDcLinkVolts(bus);
        hexagon.SetSample(newest[legVoltageA], newest[legVoltageA + 1], newest[legVoltageA + 2], newest[statorVoltageAlpha], newest[statorVoltageBeta]);
        hexagon.Paint(canvas, area);

        const auto square = Square(area);
        const Plane plane{ { square.x + square.width / 2.0f, square.y + square.height / 2.0f }, square.width / (2.0f * (2.0f / 3.0f) * bus) };
        PaintTrail(canvas, Trail(statorVoltageAlpha, statorVoltageBeta, plane), ui::theme::Current().Get(ui::theme::ColorRole::Accent));

        if (IsOvermodulated(newest[statorVoltageAlpha], newest[statorVoltageBeta], newest[busVoltage]))
        {
            canvas.SetFont(ui::theme::Current().Get(ui::theme::FontRole::StatusBold));
            canvas.SetPen({ ui::theme::Current().Get(ui::theme::ColorRole::Warning) });
            canvas.DrawText({ area.x + 8.0f, area.y + 8.0f, area.width - 16.0f, 20.0f }, ui::TextAlign::Left, ui::TextVerticalAlign::Top, "Overmodulation");
        }
    }

    void ModulationView::PaintCurrentPlane(ui::Canvas& canvas, const ui::Rect& area, const port::Sample& newest) const
    {
        const auto square = Square(area);
        Plane plane{ { square.x + square.width / 2.0f, square.y + square.height / 2.0f }, 1.0f };
        auto halfRange = std::max({ minimumCurrentRange, math::Abs(newest[currentD]), math::Abs(newest[currentQ]) });
        if (const auto first = Newest(); first.has_value())
            for (auto index = source->FirstAtOrAfter(range.start); index <= *first; ++index)
                halfRange = std::max({ halfRange, math::Abs(source->Last(index)[currentD]), math::Abs(source->Last(index)[currentQ]) });
        halfRange *= 1.1f;
        plane.scale = square.width / (2.0f * halfRange);

        PaintCurrentAxes(canvas, square, halfRange);
        const auto color = ui::theme::Current().Series(1);
        PaintTrail(canvas, Trail(currentD, currentQ, plane), color);
        canvas.SetPen({ color, 2.0f });
        canvas.SetBrush({ color });
        canvas.DrawEllipse(ToScreen(plane.centre, newest[currentD], newest[currentQ], plane.scale), 4.0f, 4.0f);
    }

    void ModulationView::PaintCurrentAxes(ui::Canvas& canvas, const ui::Rect& square, float halfRange) const
    {
        const auto& theme = ui::theme::Current();
        const ui::Point centre{ square.x + square.width / 2.0f, square.y + square.height / 2.0f };
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::GridMinor), 1.0f, ui::LineStyle::Dot });
        canvas.DrawRect(square);
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::Axis) });
        canvas.DrawLine({ square.Left(), centre.y }, { square.Right(), centre.y });
        canvas.DrawLine({ centre.x, square.Top() }, { centre.x, square.Bottom() });

        canvas.SetFont(theme.Get(ui::theme::FontRole::AxisLabel));
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::Text) });
        canvas.DrawText({ square.Right() - 60.0f, centre.y + 4.0f, 60.0f, 16.0f }, ui::TextAlign::Right, ui::TextVerticalAlign::Top, std::format("Id {:.3g} A", halfRange));
        canvas.DrawText({ centre.x + 6.0f, square.Top(), 100.0f, 16.0f }, ui::TextAlign::Left, ui::TextVerticalAlign::Top, std::format("Iq {:.3g} A", halfRange));
        canvas.DrawText({ square.Left(), square.Bottom() + 6.0f, square.width, 16.0f }, ui::TextAlign::Center, ui::TextVerticalAlign::Top, "Current in the d-q plane");
    }

    void ModulationView::PaintTrail(ui::Canvas& canvas, std::span<const ui::Point> trail, ui::Color color)
    {
        if (trail.size() < 2)
            return;
        canvas.SetAntialiasing(true);
        const auto segment = (trail.size() + trailSegments - 1) / trailSegments;
        for (std::size_t start = 0; start + 1 < trail.size(); start += segment)
        {
            const auto count = std::min(segment + 1, trail.size() - start);
            color.alpha = static_cast<uint8_t>(40 + 215 * (start + count) / trail.size());
            canvas.SetPen({ color, 1.5f });
            canvas.DrawPolyline(trail.subspan(start, count));
        }
    }
}
