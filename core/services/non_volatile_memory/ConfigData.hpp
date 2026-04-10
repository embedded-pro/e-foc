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
        float maxCurrent = 5.0f;
        float maxVelocity = 1000.0f;
        float maxTorque = 1.0f;
        uint32_t canNodeId = 1;
        uint32_t canBaudrate = 500000;
        uint32_t telemetryRateHz = 100;
        float overTempThreshold = 80.0f;
        float underVoltageThreshold = 10.0f;
        float overVoltageThreshold = 28.0f;
        uint8_t defaultControlMode = 0;
        std::array<uint8_t, 3> reserved = {};
    };

    static_assert(sizeof(ConfigData) == 40, "ConfigData layout must be free of implicit padding");

    inline ConfigData MakeDefaultConfigData()
    {
        return ConfigData{};
    }
}
