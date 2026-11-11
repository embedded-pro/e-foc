#pragma once

#include <string>
#include <vector>

namespace virtual_motor::app
{
    struct TimeTick
    {
        double time{ 0.0 };
        std::string label;
    };

    std::string TimeText(double seconds);
    std::vector<TimeTick> TimeTicks(double start, double end, double origin, std::size_t maximumTicks);
}
