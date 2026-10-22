#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace services
{
    enum class CalibrationStage : uint8_t
    {
        none = 0,
        complete = 1
    };

    inline constexpr std::size_t coggingMapBins = 48;

    // All 4-byte fields precede the byte-sized fields to avoid implicit compiler padding.
    // 'reserved1' keeps sizeof(CalibrationData) a multiple of 4 and
    // ensures the full struct is a deterministic, padding-free storage schema.
    struct CalibrationData
    {
        float rPhase = 0.0f;
        float lD = 0.0f;
        float lQ = 0.0f;
        float fluxLinkage = 0.0f;
        float currentOffsetA = 0.0f;
        float currentOffsetB = 0.0f;
        float currentOffsetC = 0.0f;
        float inertia = 0.0f;
        float frictionCoulomb = 0.0f;
        float frictionViscous = 0.0f;
        int32_t encoderZeroOffset = 0;
        float currentLoopBandwidth = 0.0f;
        float speedLoopBandwidth = 0.0f;
        float coggingTorqueScale = 0.0f;
        uint8_t encoderDirection = 0;
        uint8_t polePairs = 0;
        CalibrationStage stage = CalibrationStage::none;
        uint8_t reserved1 = 0;
        std::array<int8_t, coggingMapBins> coggingTorque{};
    };

    static_assert(sizeof(CalibrationData) == 108, "CalibrationData layout must be free of implicit padding");
}
