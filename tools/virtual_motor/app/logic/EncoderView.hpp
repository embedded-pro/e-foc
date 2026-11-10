#pragma once

#include "tools/virtual_motor/app/logic/SourceView.hpp"
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace virtual_motor::app
{
    struct EncoderGeometry
    {
        uint8_t polePairs{ 1 };
        uint32_t countsPerRevolution{ 0 };
    };

    struct EncoderStatistics
    {
        static constexpr std::size_t bins{ 41 };

        std::vector<std::pair<double, float>> difference;
        float mean{ 0.0f };
        float deviation{ 0.0f };
        float halfRange{ 1.0f };
        std::array<std::size_t, bins> histogram{};
        bool stuck{ false };
    };

    float ElectricalDifferenceDegrees(float trueAngle, float encoderAngle, uint8_t polePairs);
    EncoderStatistics MeasureEncoder(const TraceSource& source, TimeRange range, EncoderGeometry geometry);

    class EncoderView
        : public SourceView
    {
    public:
        explicit EncoderView(EncoderGeometry geometry);

        std::string_view Name() const override;
        void Show(const TraceSource* source, TimeRange range, double origin) override;
        const EncoderStatistics& Statistics() const;

        void Paint(ui::Canvas& canvas, const ui::Rect& bounds) override;
        ui::Size MinimumSize() const override;

    private:
        void PaintDifference(ui::Canvas& canvas, const ui::Rect& area) const;
        void PaintHistogram(ui::Canvas& canvas, const ui::Rect& area) const;
        void PaintReadings(ui::Canvas& canvas, const ui::Rect& area) const;

        EncoderGeometry geometry;
        TimeRange range;
        double origin{ 0.0 };
        EncoderStatistics statistics;
    };
}
