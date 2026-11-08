#pragma once

#include "core/can/FocMotorMessages.hpp"
#include <cstdint>
#include <string_view>

namespace tool
{
    // The wire carries a setpoint in SI units as a float; the operator reads speed in rpm and position in
    // degrees. A count is the smallest step offered, finer than the encoder resolves.
    class SetpointAxis
    {
    public:
        constexpr SetpointAxis(std::string_view suffix, double displayPerSi, int32_t countsPerSi, float limit, std::string_view rangeLabel)
            : suffix(suffix)
            , displayPerSi(displayPerSi)
            , countsPerSi(countsPerSi)
            , maxCounts(static_cast<int32_t>(static_cast<double>(limit) * countsPerSi))
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
        int32_t countsPerSi;
        int32_t maxCounts;
        std::string_view rangeLabel;
    };

    const SetpointAxis& TorqueAxis();
    const SetpointAxis& SpeedAxis();
    const SetpointAxis& PositionAxis();
    const SetpointAxis& AxisFor(can::FocMotorMode mode);
}
