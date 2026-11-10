#pragma once

#include "tools/virtual_motor/app/logic/SourceView.hpp"
#include <cstdint>
#include <optional>

namespace virtual_motor::app
{
    struct SpaceVector
    {
        float alpha{ 0.0f };
        float beta{ 0.0f };
    };

    SpaceVector ClarkeOf(float a, float b, float c);
    float RotorFrameSpeed(float framePeriodSeconds);

    class RotorView
        : public SourceView
    {
    public:
        explicit RotorView(uint8_t polePairs);

        std::string_view Name() const override;
        void Show(const TraceSource* source, TimeRange range, double origin) override;
        std::optional<port::Sample> Instant() const;
        bool InRotorFrame() const;

        void Paint(ui::Canvas& canvas, const ui::Rect& bounds) override;
        ui::Size MinimumSize() const override;

    private:
        struct Disc
        {
            ui::Point centre;
            float radius{ 0.0f };
            float frame{ 0.0f };
        };

        static ui::Point At(const Disc& disc, float angle, float fraction);
        void PaintStator(ui::Canvas& canvas, const Disc& disc, const port::Sample& sample) const;
        void PaintRotor(ui::Canvas& canvas, const Disc& disc, float angle) const;
        void PaintAxes(ui::Canvas& canvas, const Disc& disc, float angle) const;
        void PaintVector(ui::Canvas& canvas, const Disc& disc, SpaceVector vector, ui::Color color, std::string_view label) const;
        void PaintEncoder(ui::Canvas& canvas, const Disc& disc, float angle) const;
        void PaintCaption(ui::Canvas& canvas, const ui::Rect& bounds, const port::Sample& sample) const;

        uint8_t polePairs;
        const TraceSource* source{ nullptr };
        double origin{ 0.0 };
    };
}
