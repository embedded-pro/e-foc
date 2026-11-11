#include "tools/virtual_motor/app/logic/RotorView.hpp"
#include "numerical/math/Math.hpp"
#include "ui/theme/Theme.hpp"
#include <algorithm>
#include <array>
#include <format>
#include <numbers>
#include <vector>

namespace virtual_motor::app
{
    namespace
    {
        constexpr float pi{ std::numbers::pi_v<float> };
        constexpr float repaintPeriod{ 0.04f };
        constexpr uint8_t currentA{ 0 };
        constexpr uint8_t statorVoltageAlpha{ 12 };
        constexpr uint8_t rotorAngle{ 16 };
        constexpr uint8_t encoderAngle{ 17 };
        constexpr uint8_t rotorSpeed{ 18 };
        constexpr float statorInner{ 0.82f };
        constexpr float rotorOuter{ 0.62f };
        constexpr std::size_t arcPoints{ 12 };
        constexpr std::array<const char*, 3> phaseNames{ "a", "b", "c" };

        float Magnitude(SpaceVector vector)
        {
            return math::Hypot(vector.alpha, vector.beta);
        }
    }

    SpaceVector ClarkeOf(float a, float b, float c)
    {
        return { (2.0f / 3.0f) * (a - (b + c) / 2.0f), (b - c) * std::numbers::inv_sqrt3_v<float> };
    }

    float RotorFrameSpeed(float framePeriodSeconds)
    {
        return (pi / 2.0f) / framePeriodSeconds;
    }

    RotorView::RotorView(uint8_t polePairs)
        : polePairs(std::max<uint8_t>(polePairs, 1))
    {}

    std::string_view RotorView::Name() const
    {
        return "Rotor";
    }

    void RotorView::Show(const TraceSource* newSource, TimeRange, double newOrigin)
    {
        source = newSource;
        origin = newOrigin;
        RequestRepaint();
    }

    std::optional<port::Sample> RotorView::Instant() const
    {
        if (source == nullptr)
            return std::nullopt;
        const auto index = source->LastAtOrBefore(origin);
        return index.has_value() ? std::optional<port::Sample>{ source->Last(*index) } : std::nullopt;
    }

    bool RotorView::InRotorFrame() const
    {
        const auto sample = Instant();
        return sample.has_value() && math::Abs((*sample)[rotorSpeed]) > RotorFrameSpeed(repaintPeriod);
    }

    ui::Size RotorView::MinimumSize() const
    {
        return { 360.0f, 360.0f };
    }

    void RotorView::Paint(ui::Canvas& canvas, const ui::Rect& bounds)
    {
        canvas.FillRect(bounds, ui::theme::Current().Get(ui::theme::ColorRole::ScopeBackground));
        canvas.SetAntialiasing(true);
        const auto sample = Instant().value_or(port::Sample{});
        const auto rotorFrame = InRotorFrame();
        const Disc disc{ { bounds.x + bounds.width / 2.0f, bounds.y + bounds.height / 2.0f }, 0.42f * std::min(bounds.width, bounds.height), rotorFrame ? pi / 2.0f - sample[rotorAngle] : 0.0f };

        PaintStator(canvas, disc, sample);
        PaintRotor(canvas, disc, sample[rotorAngle]);
        PaintAxes(canvas, disc, sample[rotorAngle]);
        const auto& theme = ui::theme::Current();
        PaintVector(canvas, disc, ClarkeOf(sample[currentA], sample[currentA + 1], sample[currentA + 2]), theme.Series(1), "I");
        PaintVector(canvas, disc, { sample[statorVoltageAlpha], sample[statorVoltageAlpha + 1] }, theme.Get(ui::theme::ColorRole::Accent), "V");
        PaintEncoder(canvas, disc, sample[encoderAngle]);
        PaintCaption(canvas, bounds, sample);
    }

    ui::Point RotorView::At(const Disc& disc, float angle, float fraction)
    {
        const auto shown = angle + disc.frame;
        return { disc.centre.x + disc.radius * fraction * math::Cos(shown), disc.centre.y - disc.radius * fraction * math::Sin(shown) };
    }

    void RotorView::PaintStator(ui::Canvas& canvas, const Disc& disc, const port::Sample& sample) const
    {
        const auto& theme = ui::theme::Current();
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::Axis), 1.5f });
        canvas.SetBrush({});
        canvas.DrawEllipse(disc.centre, disc.radius, disc.radius);
        canvas.DrawEllipse(disc.centre, disc.radius * statorInner, disc.radius * statorInner);

        const auto largest = std::max({ 0.1f, math::Abs(sample[currentA]), math::Abs(sample[currentA + 1]), math::Abs(sample[currentA + 2]) });
        canvas.SetFont(theme.Get(ui::theme::FontRole::Small));
        for (uint8_t pole = 0; pole != polePairs; ++pole)
            for (uint8_t phase = 0; phase != 3; ++phase)
            {
                const auto current = sample[currentA + phase];
                auto color = theme.Get(current >= 0.0f ? ui::theme::ColorRole::Fault : ui::theme::ColorRole::Accent);
                color.alpha = static_cast<uint8_t>(40.0f + 215.0f * math::Abs(current) / largest);
                const auto angle = (static_cast<float>(phase) * 2.0f * pi / 3.0f + static_cast<float>(pole) * 2.0f * pi) / static_cast<float>(polePairs);
                canvas.SetPen({ color });
                canvas.SetBrush({ color });
                canvas.DrawEllipse(At(disc, angle, (1.0f + statorInner) / 2.0f), disc.radius * 0.06f, disc.radius * 0.06f);
                canvas.SetPen({ theme.Get(ui::theme::ColorRole::Text) });
                canvas.DrawText(At(disc, angle, 1.08f), phaseNames[phase]);
            }
    }

    void RotorView::PaintRotor(ui::Canvas& canvas, const Disc& disc, float angle) const
    {
        const auto& theme = ui::theme::Current();
        const auto poleSpan = pi / static_cast<float>(polePairs);
        for (uint8_t pole = 0; pole != 2 * polePairs; ++pole)
        {
            const auto middle = angle + static_cast<float>(pole) * poleSpan;
            std::vector<ui::Point> outline{ disc.centre };
            for (std::size_t point = 0; point <= arcPoints; ++point)
                outline.push_back(At(disc, middle - poleSpan / 2.0f + poleSpan * static_cast<float>(point) / arcPoints, rotorOuter));
            auto color = theme.Get(pole % 2 == 0 ? ui::theme::ColorRole::Fault : ui::theme::ColorRole::Accent);
            color.alpha = 70;
            canvas.SetPen({ theme.Get(ui::theme::ColorRole::GridMinor) });
            canvas.SetBrush({ color });
            canvas.DrawPolygon(outline);
        }
    }

    void RotorView::PaintAxes(ui::Canvas& canvas, const Disc& disc, float angle) const
    {
        const auto& theme = ui::theme::Current();
        const auto quarter = pi / (2.0f * static_cast<float>(polePairs));
        canvas.SetFont(theme.Get(ui::theme::FontRole::Small));
        for (const auto& [offset, name, style] : { std::tuple{ 0.0f, "d", ui::LineStyle::Solid }, std::tuple{ quarter, "q", ui::LineStyle::Dash } })
        {
            canvas.SetPen({ theme.Get(ui::theme::ColorRole::Text), 1.0f, style });
            canvas.DrawLine(disc.centre, At(disc, angle + offset, statorInner));
            canvas.DrawText(At(disc, angle + offset, statorInner * 0.9f), name);
        }
    }

    void RotorView::PaintVector(ui::Canvas& canvas, const Disc& disc, SpaceVector vector, ui::Color color, std::string_view label) const
    {
        const auto magnitude = Magnitude(vector);
        if (magnitude <= 0.0f)
            return;
        const auto angle = math::Atan2(vector.beta, vector.alpha) / static_cast<float>(polePairs);
        canvas.SetPen({ color, 2.5f });
        canvas.DrawLine(disc.centre, At(disc, angle, rotorOuter * 0.95f));
        canvas.SetPen({ color });
        canvas.DrawText(At(disc, angle, rotorOuter * 0.95f + 0.06f), std::format("{} {:.3g}", label, magnitude));
    }

    void RotorView::PaintEncoder(ui::Canvas& canvas, const Disc& disc, float angle) const
    {
        const auto& theme = ui::theme::Current();
        const std::array<ui::Point, 3> marker{ At(disc, angle, rotorOuter), At(disc, angle - 0.06f, rotorOuter + 0.1f), At(disc, angle + 0.06f, rotorOuter + 0.1f) };
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::Warning) });
        canvas.SetBrush({ theme.Get(ui::theme::ColorRole::Warning) });
        canvas.DrawPolygon(marker);
    }

    void RotorView::PaintCaption(ui::Canvas& canvas, const ui::Rect& bounds, const port::Sample& sample) const
    {
        const auto& theme = ui::theme::Current();
        canvas.SetFont(theme.Get(ui::theme::FontRole::Body));
        canvas.SetPen({ theme.Get(ui::theme::ColorRole::Text) });
        const auto speed = sample[rotorSpeed];
        canvas.DrawText({ bounds.x + 8.0f, bounds.y + 8.0f, bounds.width - 16.0f, 18.0f }, ui::TextAlign::Left, ui::TextVerticalAlign::Top,
            std::format("ω {:.4g} rad/s ({:.4g} rpm) · {} pole pairs", speed, speed * 60.0f / (2.0f * pi), polePairs));
        const auto frame = InRotorFrame() ? std::format("Rotor frame: above {:.3g} rad/s the rotor is drawn fixed, d axis up", RotorFrameSpeed(repaintPeriod))
                                          : std::string{ "Stationary frame · ▲ encoder angle · I current · V stator voltage" };
        canvas.SetPen({ theme.Get(InRotorFrame() ? ui::theme::ColorRole::Warning : ui::theme::ColorRole::TextMuted) });
        canvas.DrawText({ bounds.x + 8.0f, bounds.Bottom() - 26.0f, bounds.width - 16.0f, 18.0f }, ui::TextAlign::Left, ui::TextVerticalAlign::Top, frame);
    }
}
