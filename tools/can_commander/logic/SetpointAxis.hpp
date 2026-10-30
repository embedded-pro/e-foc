#pragma once

#include "core/can/FocMotorMessages.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string_view>

namespace tool
{
    // The wire carries a setpoint in SI units as a 16-bit count; the operator reads speed in rpm and
    // position in degrees. A count is the smallest step offered, so no value is lost in the frame.
    class SetpointAxis
    {
    public:
        constexpr SetpointAxis(std::string_view suffix, double displayPerSi, int32_t wireScale, float limit, std::string_view rangeLabel)
            : suffix(suffix)
            , displayPerSi(displayPerSi)
            , wireScale(wireScale)
            , maxCounts(static_cast<int32_t>(std::min(static_cast<double>(limit) * wireScale, static_cast<double>(std::numeric_limits<int16_t>::max()))))
            , rangeLabel(rangeLabel)
        {}

        std::string_view Suffix() const;
        std::string_view RangeLabel() const;
        int32_t MaxCounts() const;
        int Decimals() const;
        double DisplayStep() const;

        int32_t CountsOf(double displayValue) const;
        double DisplayOf(int32_t counts) const;
        double DisplayOfSi(double siValue) const;
        float SiOf(int32_t counts) const;

    private:
        std::string_view suffix;
        double displayPerSi;
        int32_t wireScale;
        int32_t maxCounts;
        std::string_view rangeLabel;
    };

    const SetpointAxis& TorqueAxis();
    const SetpointAxis& SpeedAxis();
    const SetpointAxis& PositionAxis();
    const SetpointAxis& AxisFor(can::FocMotorMode mode);
}
