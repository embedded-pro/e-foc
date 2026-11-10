#include "tools/virtual_motor/app/logic/EncoderView.hpp"
#include "numerical/math/AngleUtilities.hpp"
#include "numerical/math/Math.hpp"
#include "tools/virtual_motor/app/logic/TimeAxis.hpp"
#include "ui/theme/Theme.hpp"
#include <algorithm>
#include <format>
#include <numbers>

namespace virtual_motor::app
{
    namespace
    {
        constexpr uint8_t rotorAngle{ 16 };
        constexpr uint8_t encoderAngle{ 17 };
        constexpr double stuckWindow{ 0.01 };
        constexpr float degreesPerRadian{ 180.0f / std::numbers::pi_v<float> };
        constexpr float idealEncoderStep{ 1e-3f };

        void Summarise(EncoderStatistics& statistics)
        {
            const auto count = static_cast<float>(statistics.difference.size());
            for (const auto& entry : statistics.difference)
                statistics.mean += entry.degrees / count;
            for (const auto& entry : statistics.difference)
            {
                statistics.deviation += (entry.degrees - statistics.mean) * (entry.degrees - statistics.mean) / count;
                statistics.halfRange = std::max(statistics.halfRange, math::Abs(entry.degrees) * 1.1f);
            }
            statistics.deviation = math::Sqrt(statistics.deviation);
            for (const auto& entry : statistics.difference)
            {
                const auto position = (entry.degrees + statistics.halfRange) / (2.0f * statistics.halfRange) * static_cast<float>(EncoderStatistics::bins);
                ++statistics.histogram[std::min(EncoderStatistics::bins - 1, static_cast<std::size_t>(std::max(0.0f, position)))];
            }
        }

        bool Stuck(const TraceSource& source, std::size_t first, std::size_t last, EncoderGeometry geometry)
        {
            if (source.Time(last) - source.Time(first) < stuckWindow * 0.9)
                return false;
            const auto& start = source.Last(first);
            const auto& end = source.Last(last);
            const auto count = geometry.countsPerRevolution == 0 ? idealEncoderStep : 2.0f * std::numbers::pi_v<float> / static_cast<float>(geometry.countsPerRevolution);
            return start[encoderAngle] == end[encoderAngle] && math::Abs(math::WrapToPi(end[rotorAngle] - start[rotorAngle])) > count;
        }
    }

    float ElectricalDifferenceDegrees(float trueAngle, float encoderAngle, uint8_t polePairs)
    {
        const auto mechanical = math::WrapToPi(trueAngle - encoderAngle);
        return math::WrapToPi(mechanical * static_cast<float>(polePairs)) * degreesPerRadian;
    }

    EncoderStatistics MeasureEncoder(const TraceSource& source, TimeRange range, EncoderGeometry geometry)
    {
        EncoderStatistics statistics;
        const auto newest = source.LastAtOrBefore(range.end);
        if (!newest.has_value())
            return statistics;
        const auto first = source.FirstAtOrAfter(range.start);
        for (auto index = first; index <= *newest; ++index)
        {
            const auto& sample = source.Last(index);
            statistics.difference.emplace_back(source.Time(index), ElectricalDifferenceDegrees(sample[rotorAngle], sample[encoderAngle], geometry.polePairs));
        }
        if (statistics.difference.empty())
            return statistics;
        Summarise(statistics);
        statistics.stuck = Stuck(source, std::max(first, source.FirstAtOrAfter(source.Time(*newest) - stuckWindow)), *newest, geometry);
        return statistics;
    }

    EncoderView::EncoderView(EncoderGeometry geometry)
        : geometry(geometry)
    {}

    std::string_view EncoderView::Name() const
    {
        return "Encoder";
    }

    void EncoderView::Show(const TraceSource* source, TimeRange newRange, double newOrigin)
    {
        range = newRange;
        origin = newOrigin;
        statistics = source != nullptr ? MeasureEncoder(*source, range, geometry) : EncoderStatistics{};
        RequestRepaint();
    }

    const EncoderStatistics& EncoderView::Statistics() const
    {
        return statistics;
    }

    ui::Size EncoderView::MinimumSize() const
    {
        return { 480.0f, 360.0f };
    }

    void EncoderView::Paint(ui::Canvas& canvas, const ui::Rect& bounds)
    {
        canvas.FillRect(bounds, ui::theme::Current().Get(ui::theme::ColorRole::ScopeBackground));
        const auto top = bounds.height * 0.55f;
        PaintDifference(canvas, { bounds.x + 72.0f, bounds.y + 24.0f, bounds.width - 84.0f, top - 48.0f });
        PaintHistogram(canvas, { bounds.x + 72.0f, bounds.y + top + 24.0f, bounds.width * 0.5f - 84.0f, bounds.height - top - 48.0f });
        PaintReadings(canvas, { bounds.x + bounds.width * 0.5f + 12.0f, bounds.y + top + 24.0f, bounds.width * 0.5f - 24.0f, bounds.height - top - 48.0f });
    }

    void EncoderView::PaintDifference(ui::Canvas& canvas, const ui::Rect& area) const
    {
        const auto& theme = ui::theme::Current();
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::Axis) });
        canvas.DrawRect(area);
        canvas.SetFont(theme.Get(ui::theme::FontRole::Legend));
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::Text) });
        canvas.DrawText({ area.x, area.y - 5.0f }, "True minus encoder angle, electrical");
        canvas.DrawText({ area.x - 70.0f, area.y, 64.0f, 14.0f }, ui::TextAlign::Right, ui::TextVerticalAlign::Top, std::format("{:.3g}°e", statistics.halfRange));
        canvas.DrawText({ area.x - 70.0f, area.Bottom() - 14.0f, 64.0f, 14.0f }, ui::TextAlign::Right, ui::TextVerticalAlign::Bottom, std::format("{:.3g}°e", -statistics.halfRange));
        if (statistics.difference.empty() || range.end <= range.start)
            return;

        std::vector<ui::Point> points;
        for (const auto& [time, value] : statistics.difference)
            points.push_back({ area.x + static_cast<float>((time - range.start) / (range.end - range.start)) * area.width, area.y + area.height / 2.0f - value / statistics.halfRange * area.height / 2.0f });
        canvas.SetAntialiasing(true);
        canvas.SetPen({ theme.Series(0), 1.0f });
        canvas.DrawPolyline(points);
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::TextMuted) });
        for (const auto& tick : TimeTicks(range.start, range.end, origin, 6))
            canvas.DrawText({ area.x + static_cast<float>((tick.time - range.start) / (range.end - range.start)) * area.width - 40.0f, area.Bottom() + 2.0f, 80.0f, 14.0f }, ui::TextAlign::Center, ui::TextVerticalAlign::Top, tick.label);
    }

    void EncoderView::PaintHistogram(ui::Canvas& canvas, const ui::Rect& area) const
    {
        const auto& theme = ui::theme::Current();
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::Axis) });
        canvas.DrawRect(area);
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::Text) });
        canvas.DrawText({ area.x, area.y - 5.0f }, "Histogram of the difference");
        const auto tallest = std::max<std::size_t>(1, *std::ranges::max_element(statistics.histogram));
        const auto width = area.width / static_cast<float>(EncoderStatistics::bins);
        for (std::size_t bin = 0; bin != EncoderStatistics::bins; ++bin)
        {
            const auto height = area.height * static_cast<float>(statistics.histogram[bin]) / static_cast<float>(tallest);
            canvas.FillRect({ area.x + static_cast<float>(bin) * width, area.Bottom() - height, width - 1.0f, height }, theme.Series(0));
        }
    }

    void EncoderView::PaintReadings(ui::Canvas& canvas, const ui::Rect& area) const
    {
        const auto& theme = ui::theme::Current();
        canvas.SetFont(theme.Get(ui::theme::FontRole::Body));
        const auto resolution = geometry.countsPerRevolution == 0 ? std::string{ "Ideal encoder: no quantisation" } : std::format("{} counts per revolution · resolution {:.3g}°e", geometry.countsPerRevolution, 360.0f * static_cast<float>(geometry.polePairs) / static_cast<float>(geometry.countsPerRevolution));
        const std::array<std::string, 3> lines{
            std::format("Bias (mean)  {:.3g}°e", statistics.mean),
            std::format("Noise (σ)  {:.3g}°e", statistics.deviation),
            resolution,
        };
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::Text) });
        auto y = area.y;
        for (const auto& line : lines)
        {
            canvas.DrawText({ area.x, y, area.width, 18.0f }, ui::TextAlign::Left, ui::TextVerticalAlign::Top, line);
            y += 22.0f;
        }
        canvas.SetPen({ theme.Get(statistics.stuck ? ui::theme::ColorRole::Fault : ui::theme::ColorRole::Ok) });
        canvas.DrawText({ area.x, y, area.width, 18.0f }, ui::TextAlign::Left, ui::TextVerticalAlign::Top, statistics.stuck ? "Stuck: the encoder has not moved for 10 ms while the rotor turned" : "Following the rotor");
    }
}
