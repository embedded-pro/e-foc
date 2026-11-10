#pragma once

#include "tools/virtual_motor/app/logic/TraceSignals.hpp"
#include "tools/virtual_motor/app/logic/TraceSources.hpp"
#include "ui/core/PaintedView.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace virtual_motor::app
{
    struct TimeRange
    {
        double start{ 0.0 };
        double end{ 0.0 };
    };

    struct TraceReading
    {
        uint8_t signal{ 0 };
        std::optional<float> atA;
        std::optional<float> atB;
    };

    class TraceView
        : public ui::PaintedView
    {
    public:
        explicit TraceView(const ViewSpec& spec);

        void Show(const TraceSource* source, TimeRange range, double origin);
        void SetCursorsEnabled(bool enabled);
        void ClearCursors();
        void OnCursorsChanged(const std::function<void()>& callback);

        const ViewSpec& Spec() const;
        std::optional<double> CursorA() const;
        std::optional<double> CursorB() const;
        std::vector<TraceReading> Readings(std::size_t panel) const;
        std::string CursorSummary() const;

        void Paint(ui::Canvas& canvas, const ui::Rect& bounds) override;
        ui::Size MinimumSize() const override;
        void OnMousePress(const ui::MouseEvent& event) override;
        void OnMouseDoubleClick(const ui::MouseEvent& event) override;

    private:
        struct Column
        {
            float minimum{ 0.0f };
            float maximum{ 0.0f };
            bool breakBefore{ false };
        };

        struct Range
        {
            float low{ 0.0f };
            float high{ 0.0f };
        };

        std::vector<ui::Rect> PlotAreas(const ui::Rect& bounds) const;
        std::vector<std::optional<Column>> Columns(uint8_t signal, std::size_t count) const;
        Range VerticalRange(const PanelSpec& panel, std::size_t count) const;
        float X(double time, const ui::Rect& area) const;
        std::optional<float> ValueAt(uint8_t signal, double time) const;

        void PaintPanel(ui::Canvas& canvas, const PanelSpec& panel, const ui::Rect& area) const;
        void PaintGrid(ui::Canvas& canvas, const ui::Rect& area, Range range, const std::string& unit) const;
        void PaintTrace(ui::Canvas& canvas, uint8_t signal, std::size_t index, const ui::Rect& area, Range range) const;
        void PaintLegend(ui::Canvas& canvas, const PanelSpec& panel, const ui::Rect& area) const;
        void PaintCursors(ui::Canvas& canvas, const ui::Rect& area) const;
        void PaintReadings(ui::Canvas& canvas, std::size_t panel, const ui::Rect& area) const;
        void PaintTimeAxis(ui::Canvas& canvas, const ui::Rect& area) const;

        const ViewSpec& spec;
        const TraceSource* source{ nullptr };
        TimeRange range;
        double origin{ 0.0 };
        bool cursorsEnabled{ false };
        std::optional<double> cursorA;
        std::optional<double> cursorB;
        std::vector<ui::Rect> areas;
        std::function<void()> onCursorsChanged;
    };
}
