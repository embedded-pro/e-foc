#include "tools/can_commander/logic/FaultHistoryText.hpp"
#include <cstdio>

namespace tool
{
    std::string UptimeText(uint32_t milliseconds)
    {
        const auto hours = milliseconds / 3600000u;
        const auto minutes = milliseconds / 60000u % 60u;
        const auto seconds = milliseconds / 1000u % 60u;
        const auto fraction = milliseconds % 1000u;

        char text[32]{};
        std::snprintf(text, sizeof(text), "%u:%02u:%02u.%03u", static_cast<unsigned>(hours), static_cast<unsigned>(minutes),
            static_cast<unsigned>(seconds), static_cast<unsigned>(fraction));
        return text;
    }
}
