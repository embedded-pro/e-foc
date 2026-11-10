#include "tools/virtual_motor/app/logic/TimeAxis.hpp"
#include "numerical/math/Math.hpp"
#include <array>
#include <cstdint>
#include <format>

namespace virtual_motor::app
{
    namespace
    {
        double NiceStep(double span, std::size_t maximumTicks)
        {
            const auto rough = span / static_cast<double>(maximumTicks);
            auto decade = 1e-9;
            while (decade * 10.0 <= rough)
                decade *= 10.0;
            for (const auto factor : std::array{ 1.0, 2.0, 5.0 })
                if (decade * factor >= rough)
                    return decade * factor;
            return decade * 10.0;
        }

        std::string Trimmed(double value)
        {
            return std::format("{:.6g}", value);
        }
    }

    std::string TimeText(double seconds)
    {
        const auto magnitude = math::Abs(seconds);
        if (magnitude == 0.0)
            return "0 s";
        if (magnitude < 1e-3)
            return Trimmed(seconds * 1e6) + " µs";
        if (magnitude < 1.0)
            return Trimmed(seconds * 1e3) + " ms";
        return Trimmed(seconds) + " s";
    }

    std::vector<TimeTick> TimeTicks(double start, double end, double origin, std::size_t maximumTicks)
    {
        std::vector<TimeTick> ticks;
        if (end <= start || maximumTicks == 0)
            return ticks;

        const auto step = NiceStep(end - start, maximumTicks);
        const auto first = static_cast<int64_t>(math::Ceil((start - origin) / step - 1e-9));
        for (auto multiple = first; origin + static_cast<double>(multiple) * step <= end + step * 1e-9; ++multiple)
        {
            const auto fromOrigin = static_cast<double>(multiple) * step;
            ticks.emplace_back(origin + fromOrigin, TimeText(fromOrigin));
        }
        return ticks;
    }
}
