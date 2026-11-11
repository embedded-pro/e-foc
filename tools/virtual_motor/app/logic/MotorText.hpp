#pragma once

#include "tools/virtual_motor/port/PlantPortMessages.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace virtual_motor::app
{
    struct RealTime
    {
        std::string text;
        bool late{ false };
        std::string detail;
    };

    enum class ChangeKind : uint8_t
    {
        loadTorque,
        externalTorque,
        currentNoise,
        encoderNoise,
    };

    struct ActiveChange
    {
        ChangeKind kind;
        std::string text;
    };

    RealTime RealTimeText(const port::Status& status);
    std::string LedText(uint8_t leds);
    std::string ProtectionText(uint8_t protection);
    std::string VirtualTimeText(uint64_t controlPeriods, uint32_t controlRateHz);
    std::string ExternalTorqueText(const port::ExternalTorque& torque);
    std::vector<ActiveChange> ActiveChanges(const port::Changes& changes);
    std::string PrintableLogLine(const std::string& line);
    std::string InForceText(const port::Changes& changes);
}
