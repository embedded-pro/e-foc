#pragma once

#include <algorithm>
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

    // Padding-free schema: four-byte fields first, the current noise completes the word; see service-nvm.md.
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
        uint8_t currentNoiseMilliAmpere = 0;
        std::array<int8_t, coggingMapBins> coggingTorque{};
    };

    static_assert(sizeof(CalibrationData) == 108, "CalibrationData layout must be free of implicit padding");

    inline uint8_t CurrentNoiseMilliAmpere(float noiseAmpere)
    {
        return static_cast<uint8_t>(std::clamp(noiseAmpere * 1000.0f + 0.5f, 0.0f, 255.0f));
    }

    inline float CurrentNoiseAmpere(uint8_t noiseMilliAmpere)
    {
        return static_cast<float>(noiseMilliAmpere) * 0.001f;
    }

    enum class MotorParameter : uint8_t
    {
        resistance,
        inductance,
        polePairs,
        fluxLinkage,
        inertia,
        viscousFriction
    };

    inline float ReadMotorParameter(const CalibrationData& data, MotorParameter parameter)
    {
        switch (parameter)
        {
            case MotorParameter::resistance:
                return data.rPhase;
            case MotorParameter::inductance:
                return data.lD;
            case MotorParameter::polePairs:
                return static_cast<float>(data.polePairs);
            case MotorParameter::fluxLinkage:
                return data.fluxLinkage;
            case MotorParameter::inertia:
                return data.inertia;
            default:
                return data.frictionViscous;
        }
    }

    inline void WriteMotorParameter(CalibrationData& data, MotorParameter parameter, float value)
    {
        switch (parameter)
        {
            case MotorParameter::resistance:
                data.rPhase = value;
                break;
            case MotorParameter::inductance:
                data.lD = value;
                data.lQ = value;
                break;
            case MotorParameter::polePairs:
                data.polePairs = static_cast<uint8_t>(value);
                break;
            case MotorParameter::fluxLinkage:
                data.fluxLinkage = value;
                break;
            case MotorParameter::inertia:
                data.inertia = value;
                break;
            default:
                data.frictionViscous = value;
                break;
        }
    }
}
