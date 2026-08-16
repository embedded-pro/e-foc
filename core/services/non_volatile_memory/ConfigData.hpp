#pragma once

#include <cstdint>

namespace services
{
    // All 4-byte fields precede the byte-sized fields to avoid implicit compiler padding.
    struct ConfigData
    {
        uint32_t canNodeId = 1;
        uint32_t canBaudrate = 1000000;
        uint32_t telemetryRateHz = 100;
        uint32_t encoderResolution = 4000;
        uint8_t defaultControlMode = 0;
        uint8_t currentAlgorithm = 0;
        uint8_t speedAlgorithm = 0;
        uint8_t positionAlgorithm = 0;
    };

    static_assert(sizeof(ConfigData) == 20, "ConfigData layout must be free of implicit padding");

    inline ConfigData MakeDefaultConfigData()
    {
        return ConfigData{};
    }
}
