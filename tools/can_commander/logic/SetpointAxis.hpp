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
    // A precise axis steps finer than that count, for a controller that takes a setpoint as a float.
    class SetpointAxis
    {
    public:
        constexpr SetpointAxis(std::string_view suffix, double displayPerSi, int32_t wireScale, float limit, std::string_view rangeLabel, int32_t countCap = std::numeric_limits<int16_t>::max())
            : suffix(suffix)
            , displayPerSi(displayPerSi)
            , wireScale(wireScale)
            , maxCounts(static_cast<int32_t>(std::min(static_cast<double>(limit) * wireScale, static_cast<double>(countCap))))
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
    const SetpointAxis& PreciseAxisFor(can::FocMotorMode mode);
    const SetpointAxis& AxisFor(can::FocMotorMode mode);
    const SetpointAxis& AxisFor(can::FocMotorMode mode, bool precise);
}
