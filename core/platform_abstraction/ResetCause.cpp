#include "core/platform_abstraction/ResetCause.hpp"
#include "infra/stream/StreamManipulators.hpp"

namespace application
{
    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, ResetCause cause)
    {
        switch (cause)
        {
            case ResetCause::powerUp:
                stream << "PowerUp";
                break;
            case ResetCause::brownOut:
                stream << "BrownOut";
                break;
            case ResetCause::software:
                stream << "Software";
                break;
            case ResetCause::hardware:
                stream << "Hardware";
                break;
            case ResetCause::watchdog:
                stream << "Watchdog";
                break;
            default:
                stream << "Unknown";
                break;
        }
        return stream;
    }
}
