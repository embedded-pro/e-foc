#pragma once

#include "core/foc/interfaces/LoopTunings.hpp"
#include <array>
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
        float currentBandwidth = 0.0f;
        float currentSwitchingGain = foc::CurrentLoopTunings{}.switchingGain;
        float currentBoundaryLayer = foc::CurrentLoopTunings{}.boundaryLayer;
        float speedBandwidth = 0.0f;
        float speedErrorWeight = foc::SpeedLoopTunings{}.speedErrorWeight;
        float speedIntegralWeight = foc::SpeedLoopTunings{}.integralWeight;
        float speedObserverBandwidthRatio = foc::SpeedLoopTunings{}.observerBandwidthRatio;
        float speedReferenceTimeConstant = foc::SpeedLoopTunings{}.referenceTimeConstant;
        float positionBandwidth = 0.0f;
        float positionErrorWeight = foc::PositionLoopTunings{}.positionErrorWeight;
        float positionSpeedErrorWeight = foc::PositionLoopTunings{}.speedErrorWeight;
        float positionIntegralWeight = foc::PositionLoopTunings{}.integralWeight;
        float positionReferenceTimeConstant = foc::PositionLoopTunings{}.referenceTimeConstant;
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
