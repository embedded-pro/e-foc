#pragma once

#include <array>
#include <cstdint>

namespace services
{
    // All 4-byte fields precede the byte-sized fields to avoid implicit compiler padding.
    // 'reserved' is explicit padding to keep sizeof(ConfigData) a multiple of 4 and
    // ensure the full struct is a deterministic, padding-free storage schema.
    struct ConfigData
    {
        uint32_t canNodeId = 1;
        uint32_t canBaudrate = 1000000;
        uint32_t telemetryRateHz = 100;
        uint32_t encoderResolution = 4000;
        uint8_t defaultControlMode = 0;
        std::array<uint8_t, 3> reserved = {};
    };

    static_assert(sizeof(ConfigData) == 20, "ConfigData layout must be free of implicit padding");

    inline ConfigData MakeDefaultConfigData()
    {
        return ConfigData{};
    }
}
