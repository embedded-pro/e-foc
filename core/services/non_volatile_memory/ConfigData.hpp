#pragma once

#include "core/foc/interfaces/LoopTunings.hpp"
#include <array>
#include <cstdint>

namespace services
{
    struct CurrentLoopSettings
    {
        float bandwidth = 0.0f;
        float switchingGain = foc::CurrentLoopTunings{}.switchingGain;
        float boundaryLayer = foc::CurrentLoopTunings{}.boundaryLayer;
    };

    struct SpeedLoopSettings
    {
        float bandwidth = 0.0f;
        float errorWeight = foc::SpeedLoopTunings{}.speedErrorWeight;
        float integralWeight = foc::SpeedLoopTunings{}.integralWeight;
        float observerBandwidthRatio = foc::SpeedLoopTunings{}.observerBandwidthRatio;
        float referenceTimeConstant = foc::SpeedLoopTunings{}.referenceTimeConstant;
    };

    struct PositionLoopSettings
    {
        float bandwidth = 0.0f;
        float errorWeight = foc::PositionLoopTunings{}.positionErrorWeight;
        float speedErrorWeight = foc::PositionLoopTunings{}.speedErrorWeight;
        float integralWeight = foc::PositionLoopTunings{}.integralWeight;
        float referenceTimeConstant = foc::PositionLoopTunings{}.referenceTimeConstant;
    };

    // All 4-byte fields precede the byte-sized fields to avoid implicit compiler padding.
    struct ConfigData
    {
        uint32_t canNodeId = 1;
        uint32_t canBaudrate = 1000000;
        uint32_t telemetryRateHz = 100;
        uint32_t encoderResolution = 4000;
        CurrentLoopSettings currentLoop;
        SpeedLoopSettings speedLoop;
        PositionLoopSettings positionLoop;
        uint8_t defaultControlMode = 0;
        uint8_t currentAlgorithm = 0;
        uint8_t speedAlgorithm = 0;
        uint8_t positionAlgorithm = 0;
        uint8_t currentTwoStepDeadbeat = 0;
        std::array<uint8_t, 3> reserved{};
    };

    static_assert(sizeof(ConfigData) == 76, "ConfigData layout must be free of implicit padding");

    inline ConfigData MakeDefaultConfigData()
    {
        return ConfigData{};
    }
}
