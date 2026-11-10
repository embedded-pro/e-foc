#include "tools/virtual_motor/app/logic/TraceView.hpp"
#include "numerical/math/Math.hpp"
#include "tools/virtual_motor/app/logic/TimeAxis.hpp"
#include "ui/theme/Theme.hpp"
#include <algorithm>
#include <format>

namespace virtual_motor::app
{
    namespace
    {
        constexpr float leftMargin{ 88.0f };
        constexpr float rightMargin{ 12.0f };
        constexpr float titleHeight{ 18.0f };
        constexpr float panelGap{ 8.0f };
        constexpr float axisHeight{ 22.0f };
        constexpr std::size_t horizontalDivisions{ 4 };
        constexpr uint8_t envelopeAlpha{ 70 };

        std::string Number(float value)
        {
            return std::format("{:.4g}", value);
        }

        std::string Reading(std::optional<float> value)
        {
            return value.has_value() ? Number(*value) : std::string{ "—" };
        }

        std::string AxisNumber(float value, float step)
        {
            const auto decimals = std::clamp(static_cast<int>(math::Ceil(-math::Log10(step))) + 1, 0, 6);
            const auto shown = math::Abs(value) < step * 1e-3f ? 0.0f : value;
            return std::format("{:.{}f}", shown, decimals);
        }

        ui::Color Faded(ui::Color color)
        {
            color.alpha = envelopeAlpha;
            return color;
        }

        float Y(float value, const ui::Rect& area, float low, float high)
        {
            return area.Bottom() - (value - low) / (high - low) * area.height;
        }
    }

    TraceView::TraceView(const ViewSpec& spec)
        : spec(spec)
    {}

    void TraceView::Show(const TraceSource* newSource, TimeRange newRange, double newOrigin)
    {
        source = newSource;
        range = newRange;
        origin = newOrigin;
        RequestRepaint();
    }

    void TraceView::SetCursorsEnabled(bool enabled)
    {
        cursorsEnabled = enabled;
        if (!enabled)
            ClearCursors();
    }

    void TraceView::ClearCursors()
    {
        if (!cursorA.has_value() && !cursorB.has_value())
            return;
        cursorA.reset();
        cursorB.reset();
        RequestRepaint();
        if (onCursorsChanged)
            onCursorsChanged();
    }

    void TraceView::OnCursorsChanged(const std::function<void()>& callback)
    {
        onCursorsChanged = callback;
    }

    const ViewSpec& TraceView::Spec() const
    {
        return spec;
    }

    std::optional<double> TraceView::CursorA() const
    {
        return cursorA;
    }

    std::optional<double> TraceView::CursorB() const
    {
        return cursorB;
    }

    std::vector<TraceReading> TraceView::Readings(std::size_t panel) const
    {
        std::vector<TraceReading> readings;
        for (const auto signal : spec.panels[panel].traces)
            readings.push_back({ signal, cursorA.has_value() ? ValueAt(signal, *cursorA) : std::nullopt, cursorB.has_value() ? ValueAt(signal, *cursorB) : std::nullopt });
        return readings;
    }

    std::string TraceView::CursorSummary() const
    {
        if (!cursorA.has_value() && !cursorB.has_value())
            return cursorsEnabled ? "Cursors: left click places A, right click B, double click removes them" : "";

        std::string summary;
        if (cursorA.has_value())
            summary += "A " + TimeText(*cursorA - origin);
        if (cursorB.has_value())
            summary += (summary.empty() ? "" : "   ") + std::string{ "B " } + TimeText(*cursorB - origin);
        if (cursorA.has_value() && cursorB.has_value() && *cursorB != *cursorA)
        {
            const auto delta = *cursorB - *cursorA;
            summary += std::format("   Δt {}   1/Δt {:.4g} Hz", TimeText(delta), 1.0 / math::Abs(delta));
        }
        return summary;
    }

    ui::Size TraceView::MinimumSize() const
    {
        return { 320.0f, 120.0f * static_cast<float>(spec.panels.size()) };
    }

    void TraceView::Paint(ui::Canvas& canvas, const ui::Rect& bounds)
    {
        const auto& theme = ui::theme::Current();
        canvas.FillRect(bounds, theme.Get(ui::theme::ColorRole::ScopeBackground));
        areas = PlotAreas(bounds);
        for (std::size_t panel = 0; panel != areas.size(); ++panel)
        {
            PaintPanel(canvas, spec.panels[panel], areas[panel]);
            PaintCursors(canvas, areas[panel]);
            PaintReadings(canvas, panel, areas[panel]);
        }
        if (!areas.empty())
            PaintTimeAxis(canvas, areas.back());
    }

    void TraceView::OnMousePress(const ui::MouseEvent& event)
    {
        if (!cursorsEnabled || range.end <= range.start)
            return;
        const auto area = std::ranges::find_if(areas, [&event](const ui::Rect& candidate)
            {
                return candidate.Contains(event.position);
            });
        if (area == areas.end())
            return;

        const auto time = range.start + (event.position.x - area->Left()) / area->width * (range.end - range.start);
        if (event.button == ui::MouseButton::Left)
            cursorA = time;
        else if (event.button == ui::MouseButton::Right)
            cursorB = time;
        else
            return;
        RequestRepaint();
        if (onCursorsChanged)
            onCursorsChanged();
    }

    void TraceView::OnMouseDoubleClick(const ui::MouseEvent&)
    {
        ClearCursors();
    }

    std::vector<ui::Rect> TraceView::PlotAreas(const ui::Rect& bounds) const
    {
        std::vector<ui::Rect> result;
        const auto panels = static_cast<float>(spec.panels.size());
        const auto available = bounds.height - axisHeight - panelGap * (panels - 1.0f);
        const auto panelHeight = std::max(0.0f, available / panels);
        for (std::size_t panel = 0; panel != spec.panels.size(); ++panel)
        {
            const auto top = bounds.Top() + static_cast<float>(panel) * (panelHeight + panelGap);
            result.push_back({ bounds.Left() + leftMargin, top + titleHeight, std::max(0.0f, bounds.width - leftMargin - rightMargin), std::max(0.0f, panelHeight - titleHeight) });
        }
        return result;
    }

    std::vector<std::optional<TraceView::Column>> TraceView::Columns(uint8_t signal, std::size_t count) const
    {
        std::vector<std::optional<Column>> columns(count);
        if (source == nullptr || count == 0 || range.end <= range.start)
            return columns;

        bool pendingBreak{ false };
        for (auto index = source->FirstAtOrAfter(range.start); index < source->Count() && source->Time(index) <= range.end; ++index)
        {
            const auto position = (source->Time(index) - range.start) / (range.end - range.start) * static_cast<double>(count);
            const auto column = std::min(count - 1, static_cast<std::size_t>(position));
            pendingBreak = pendingBreak || source->BreakBefore(index);
            auto& entry = columns[column];
            if (!entry)
                entry = Column{ source->Minimum(index, signal), source->Maximum(index, signal), pendingBreak };
            else
            {
                entry->minimum = std::min(entry->minimum, source->Minimum(index, signal));
                entry->maximum = std::max(entry->maximum, source->Maximum(index, signal));
            }
            pendingBreak = false;
        }
        return columns;
    }

    TraceView::Range TraceView::VerticalRange(const PanelSpec& panel, std::size_t count) const
    {
        std::optional<Range> found;
        for (const auto signal : panel.traces)
            for (const auto& column : Columns(signal, count))
                if (column)
                    found = found ? Range{ std::min(found->low, column->minimum), std::max(found->high, column->maximum) } : Range{ column->minimum, column->maximum };

        auto result = found.value_or(Range{ -1.0f, 1.0f });
        const auto span = result.high - result.low;
        const auto margin = span > 0.0f ? span * 0.05f : std::max(1e-3f, math::Abs(result.high) * 0.1f);
        return { result.low - margin, result.high + margin };
    }

    float TraceView::X(double time, const ui::Rect& area) const
    {
        return area.Left() + static_cast<float>((time - range.start) / (range.end - range.start)) * area.width;
    }

    std::optional<float> TraceView::ValueAt(uint8_t signal, double time) const
    {
        if (source == nullptr || source->Count() == 0)
            return std::nullopt;
        auto index = source->FirstAtOrAfter(time);
        const bool exact = index != source->Count() && source->Time(index) == time;
        if (!exact && index == 0)
            return std::nullopt;
        if (!exact)
            --index;
        return (source->Minimum(index, signal) + source->Maximum(index, signal)) / 2.0f;
    }

    void TraceView::PaintPanel(ui::Canvas& canvas, const PanelSpec& panel, const ui::Rect& area) const
    {
        const auto count = static_cast<std::size_t>(std::max(1.0f, area.width));
        const auto vertical = VerticalRange(panel, count);
        PaintGrid(canvas, area, vertical, panel.unit);
        ui::CanvasStateGuard guard{ canvas };
        canvas.SetClip(area);
        canvas.SetAntialiasing(true);
        for (std::size_t index = 0; index != panel.traces.size(); ++index)
            PaintTrace(canvas, panel.traces[index], index, area, vertical);
        canvas.ClearClip();
        PaintLegend(canvas, panel, area);
    }

    void TraceView::PaintGrid(ui::Canvas& canvas, const ui::Rect& area, Range vertical, std::string_view unit) const
    {
        const auto& theme = ui::theme::Current();
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::GridMinor), 1.0f, ui::LineStyle::Dot });
        for (const auto& tick : TimeTicks(range.start, range.end, origin, 8))
            canvas.DrawLine({ X(tick.time, area), area.Top() }, { X(tick.time, area), area.Bottom() });

        canvas.SetFont(theme.Get(ui::theme::FontRole::AxisLabel));
        for (std::size_t division = 0; division <= horizontalDivisions; ++division)
        {
            const auto fraction = static_cast<float>(division) / static_cast<float>(horizontalDivisions);
            const auto y = area.Bottom() - fraction * area.height;
            canvas.SetPen({ theme.Get(ui::theme::ColorRole::GridMinor), 1.0f, ui::LineStyle::Dot });
            canvas.DrawLine({ area.Left(), y }, { area.Right(), y });
            canvas.SetPen({ theme.Get(ui::theme::ColorRole::TextMuted) });
            const auto value = vertical.low + fraction * (vertical.high - vertical.low);
            const auto step = (vertical.high - vertical.low) / static_cast<float>(horizontalDivisions);
            canvas.DrawText({ area.Left() - leftMargin, y - 8.0f, leftMargin - 6.0f, 16.0f }, ui::TextAlign::Right, ui::TextVerticalAlign::Middle, AxisNumber(value, step) + (unit == "1" ? std::string{} : std::format(" {}", unit)));
        }
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::Axis) });
        canvas.DrawRect(area);
    }

    void TraceView::PaintTrace(ui::Canvas& canvas, uint8_t signal, std::size_t index, const ui::Rect& area, Range vertical) const
    {
        const auto color = ui::theme::Current().Series(index);
        canvas.SetPen({ color, 1.0f });
        canvas.SetBrush({ Faded(color) });

        const auto columns = Columns(signal, static_cast<std::size_t>(std::max(1.0f, area.width)));
        std::vector<ui::Point> upper;
        std::vector<ui::Point> lower;
        const auto flush = [&canvas, &upper, &lower]()
        {
            if (upper.empty())
                return;
            std::ranges::reverse(lower);
            upper.insert(upper.end(), lower.begin(), lower.end());
            canvas.DrawPolygon(upper);
            upper.clear();
            lower.clear();
        };

        for (std::size_t column = 0; column != columns.size(); ++column)
        {
            if (!columns[column])
                continue;
            if (columns[column]->breakBefore)
                flush();
            const auto x = area.Left() + static_cast<float>(column) + 0.5f;
            upper.push_back({ x, Y(columns[column]->maximum, area, vertical.low, vertical.high) });
            lower.push_back({ x, Y(columns[column]->minimum, area, vertical.low, vertical.high) });
        }
        flush();
    }

    void TraceView::PaintLegend(ui::Canvas& canvas, const PanelSpec& panel, const ui::Rect& area) const
    {
        const auto& theme = ui::theme::Current();
        canvas.SetFont(theme.Get(ui::theme::FontRole::Legend));
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::Text) });
        auto x = area.Left();
        const auto baseline = area.Top() - 5.0f;
        canvas.DrawText({ x, baseline }, panel.title);
        x += canvas.MeasureText(panel.title).width + 16.0f;
        for (std::size_t index = 0; index != panel.traces.size(); ++index)
        {
            const auto name = Signals()[panel.traces[index]].name;
            canvas.SetPen({ theme.Series(index) });
            canvas.DrawText({ x, baseline }, name);
            x += canvas.MeasureText(name).width + 12.0f;
        }
    }

    void TraceView::PaintCursors(ui::Canvas& canvas, const ui::Rect& area) const
    {
        const auto& theme = ui::theme::Current();
        canvas.SetFont(theme.Get(ui::theme::FontRole::Small));
        for (const auto& [cursor, name, style] : { std::tuple{ cursorA, "A", ui::LineStyle::Solid }, std::tuple{ cursorB, "B", ui::LineStyle::Dash } })
        {
            if (!cursor.has_value() || *cursor < range.start || *cursor > range.end)
                continue;
            const auto x = X(*cursor, area);
            canvas.SetPen({ theme.Get(ui::theme::ColorRole::Crosshair), 1.0f, style });
            canvas.DrawLine({ x, area.Top() }, { x, area.Bottom() });
            canvas.DrawText({ x + 3.0f, area.Top() + 12.0f }, name);
        }
    }

    void TraceView::PaintReadings(ui::Canvas& canvas, std::size_t panel, const ui::Rect& area) const
    {
        if (!cursorA.has_value() && !cursorB.has_value())
            return;
        const auto& theme = ui::theme::Current();
        canvas.SetFont(theme.Get(ui::theme::FontRole::Small));
        auto y = area.Top() + 14.0f;
        const auto readings = Readings(panel);
        for (std::size_t index = 0; index != readings.size(); ++index)
        {
            const auto& reading = readings[index];
            auto text = std::format("{}  A {}  B {}", Signals()[reading.signal].name, Reading(reading.atA), Reading(reading.atB));
            if (reading.atA.has_value() && reading.atB.has_value())
                text += "  Δ " + Number(*reading.atB - *reading.atA);
            canvas.SetPen({ theme.Series(index) });
            canvas.DrawText({ area.Left(), y - 12.0f, area.width - 6.0f, 14.0f }, ui::TextAlign::Right, ui::TextVerticalAlign::Middle, text);
            y += 14.0f;
        }
    }

    void TraceView::PaintTimeAxis(ui::Canvas& canvas, const ui::Rect& area) const
    {
        const auto& theme = ui::theme::Current();
        canvas.SetFont(theme.Get(ui::theme::FontRole::AxisLabel));
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::TextMuted) });
        for (const auto& tick : TimeTicks(range.start, range.end, origin, 8))
        {
            const auto x = X(tick.time, area);
            canvas.DrawText({ x - 40.0f, area.Bottom() + 4.0f, 80.0f, axisHeight - 6.0f }, ui::TextAlign::Center, ui::TextVerticalAlign::Top, tick.label);
        }
    }
}
