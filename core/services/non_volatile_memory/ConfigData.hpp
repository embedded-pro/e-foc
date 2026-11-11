#pragma once

#include "core/foc/interfaces/LoopTunings.hpp"
#include <cstdint>
#include <numbers>

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

    struct SupervisionSettings
    {
        float window = 0.1f;
        float currentFraction = 0.05f;
        float demandedSpeed = 2.0f;
        float positionError = 0.2f;
        float stillExcursion = 0.01f;
    };

    struct LimitSettings
    {
        float overspeedLimit = 1000.0f;
        float followingErrorWindow = 0.0f;
        float followingErrorTime = 0.1f;
        float speedAcceleration = 0.0f;
        float speedDeceleration = 0.0f;
        float profileVelocity = 0.0f;
        float profileAcceleration = 0.0f;
        float profileDeceleration = 0.0f;
        float profileJerk = 0.0f;
        float commandTimeout = 0.0f;
        float continuousCurrent = 0.0f;
        float peakCurrent = 0.0f;
        float peakCurrentTime = 1.0f;
        float positionWindow = 2.0f * std::numbers::pi_v<float> * 4.0f / 4000.0f;
        float positionWindowTime = 0.02f;
        float positionMinimum = 0.0f;
        float positionMaximum = 0.0f;
        float calibrationTemperature = 25.0f;
        float temperatureLimit = 120.0f;
        float temperatureMargin = 15.0f;
        float busVoltageMinimum = 0.0f;
        float busVoltageMaximum = 0.0f;
    };

    enum class RecalibrationMode : uint8_t
    {
        off = 0,
        onCommand = 1,
        automatic = 2
    };

    inline constexpr uint32_t ConfigMagic = 0xDEADBEEF;
    inline constexpr uint8_t ConfigLayoutVersion = 15;

    // Padding-free storage schema: four-byte fields first; see service-nvm.md.
    struct ConfigData
    {
        uint32_t canNodeId = 1;
        uint32_t canBaudrate = 1000000;
        uint32_t telemetryRateHz = 100;
        uint32_t encoderResolution = 4000;
        CurrentLoopSettings currentLoop;
        SpeedLoopSettings speedLoop;
        PositionLoopSettings positionLoop;
        SupervisionSettings supervision;
        LimitSettings limits;
        uint8_t defaultControlMode = 0;
        uint8_t currentAlgorithm = 0;
        uint8_t speedAlgorithm = 0;
        uint8_t positionAlgorithm = 0;
        uint8_t currentTwoStepDeadbeat = 0;
        uint8_t stallDetection = 1;
        uint8_t recalibrationMode = 0;
        uint8_t homingRequired = 0;
    };

    static_assert(sizeof(ConfigData) == 184, "ConfigData layout must be free of implicit padding");
}
