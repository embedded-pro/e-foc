#include "tools/can_commander/logic/FaultHistoryText.hpp"
#include <format>

namespace tool
{
    std::string UptimeText(uint32_t milliseconds)
    {
        const auto hours = milliseconds / 3600000u;
        const auto minutes = milliseconds / 60000u % 60u;
        const auto seconds = milliseconds / 1000u % 60u;
        const auto fraction = milliseconds % 1000u;

        return std::format("{}:{:02}:{:02}.{:03}", hours, minutes, seconds, fraction);
    }
}
