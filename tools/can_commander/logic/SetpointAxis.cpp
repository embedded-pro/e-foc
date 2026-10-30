#include "tools/can_commander/logic/SetpointAxis.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace tool
{
    namespace
    {
        constexpr double rpmPerRadPerSecond{ 60.0 / (2.0 * std::numbers::pi) };
        constexpr double degreesPerRadian{ 180.0 / std::numbers::pi };
        constexpr const char* firmwareRange{ "Accepted by the firmware" };

        constexpr float halfTurn{ std::numbers::pi_v<float> };
    }

    SetpointAxis::SetpointAxis(std::string_view suffix, double displayPerSi, int32_t wireScale, float limit, std::string_view rangeLabel)
        : suffix(suffix)
        , displayPerSi(displayPerSi)
        , wireScale(wireScale)
        , maxCounts(static_cast<int32_t>(std::min(std::floor(static_cast<double>(limit) * wireScale), static_cast<double>(std::numeric_limits<int16_t>::max()))))
        , rangeLabel(rangeLabel)
    {}

    std::string_view SetpointAxis::Suffix() const
    {
        return suffix;
    }

    std::string_view SetpointAxis::RangeLabel() const
    {
        return rangeLabel;
    }

    int32_t SetpointAxis::MaxCounts() const
    {
        return maxCounts;
    }

    int SetpointAxis::Decimals() const
    {
        // The epsilon keeps a step of exactly 0.1 at one decimal despite log10 rounding.
        return std::max(0, static_cast<int>(std::ceil(-std::log10(DisplayStep()) - 1e-9)));
    }

    double SetpointAxis::DisplayStep() const
    {
        return displayPerSi / wireScale;
    }

    int32_t SetpointAxis::CountsOf(double displayValue) const
    {
        const auto counts = std::lround(displayValue / DisplayStep());
        return static_cast<int32_t>(std::clamp<long>(counts, -maxCounts, maxCounts));
    }

    double SetpointAxis::DisplayOf(int32_t counts) const
    {
        return counts * DisplayStep();
    }

    double SetpointAxis::DisplayOfSi(double siValue) const
    {
        return siValue * displayPerSi;
    }

    float SetpointAxis::SiOf(int32_t counts) const
    {
        return static_cast<float>(static_cast<double>(counts) / wireScale);
    }

    const SetpointAxis& TorqueAxis()
    {
        static const SetpointAxis axis{ " A", 1.0, can::focCurrentScale, foc::CommandLimits::maxTorqueSetpoint, firmwareRange };
        return axis;
    }

    const SetpointAxis& SpeedAxis()
    {
        static const SetpointAxis axis{ " rpm", rpmPerRadPerSecond, can::focSpeedScale, foc::CommandLimits::maxSpeedSetpoint, firmwareRange };
        return axis;
    }

    const SetpointAxis& PositionAxis()
    {
        static const SetpointAxis axis{ "\xC2\xB0", degreesPerRadian, can::focPositionScale, halfTurn, "One turn from the alignment zero" };
        return axis;
    }

    const SetpointAxis& AxisFor(can::FocMotorMode mode)
    {
        switch (mode)
        {
            case can::FocMotorMode::speed:
                return SpeedAxis();
            case can::FocMotorMode::position:
                return PositionAxis();
            default:
                return TorqueAxis();
        }
    }
}
