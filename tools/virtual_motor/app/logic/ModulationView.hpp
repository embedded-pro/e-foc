#pragma once

#include "tools/virtual_motor/app/logic/SourceView.hpp"
#include "ui/widgets/HexagonCore.hpp"
#include <optional>
#include <span>
#include <vector>

namespace virtual_motor::app
{
    bool IsOvermodulated(float vAlpha, float vBeta, float busVolts);

    class ModulationView
        : public SourceView
    {
    public:
        std::string_view Name() const override;
        void Show(const TraceSource* source, TimeRange range, double origin) override;

        void Paint(ui::Canvas& canvas, const ui::Rect& bounds) override;
        ui::Size MinimumSize() const override;

    private:
        struct Plane
        {
            ui::Point centre;
            float scale{ 1.0f };
        };

        std::optional<std::size_t> Newest() const;
        std::vector<ui::Point> Trail(uint8_t x, uint8_t y, const Plane& plane) const;
        void PaintHexagon(ui::Canvas& canvas, const ui::Rect& area, const port::Sample& newest);
        void PaintCurrentPlane(ui::Canvas& canvas, const ui::Rect& area, const port::Sample& newest) const;
        void PaintCurrentAxes(ui::Canvas& canvas, const ui::Rect& square, float halfRange) const;
        static void PaintTrail(ui::Canvas& canvas, std::span<const ui::Point> trail, ui::Color color);

        ui::widgets::HexagonCore hexagon;
        const TraceSource* source{ nullptr };
        TimeRange range;
    };
}
