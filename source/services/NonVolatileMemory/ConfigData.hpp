#pragma once

#include <cstdint>

namespace services
{
    struct ConfigData
    {
        uint8_t defaultControlMode = 0;
        float maxCurrent = 5.0f;
        float maxVelocity = 1000.0f;
        float maxTorque = 1.0f;
        uint32_t canNodeId = 1;
        uint32_t canBaudrate = 500000;
        uint32_t telemetryRateHz = 100;
        float overTempThreshold = 80.0f;
        float underVoltageThreshold = 10.0f;
        float overVoltageThreshold = 28.0f;
    };

    inline ConfigData MakeDefaultConfigData()
    {
        return ConfigData{};
    }
}
