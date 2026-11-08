#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>

namespace services
{
    enum class CalibrationStage : uint8_t
    {
        none = 0,
        complete = 1
    };

    inline constexpr std::size_t coggingMapBins = 48;

    inline constexpr uint32_t CalibrationMagic = 0xCAFEF00D;
    inline constexpr uint8_t CalibrationLayoutVersion = 6;

    struct CalibrationData
    {
        float rPhase = 0.0f;
        float lD = 0.0f;
        float fluxLinkage = 0.0f;
        float currentOffsetA = 0.0f;
        float currentOffsetB = 0.0f;
        float currentOffsetC = 0.0f;
        float inertia = 0.0f;
        float frictionCoulomb = 0.0f;
        float frictionViscous = 0.0f;
        float speedLoopBandwidth = 0.0f;
        float coggingTorqueScale = 0.0f;
        // Mechanical angle of the encoder index in the aligned frame; NaN when none was found
        float encoderIndexAngle = std::numeric_limits<float>::quiet_NaN();
        uint16_t currentNoiseMilliAmpere = 0;
        uint8_t polePairs = 0;
        CalibrationStage stage = CalibrationStage::none;
        std::array<int8_t, coggingMapBins> coggingTorque{};
    };

    static_assert(sizeof(CalibrationData) == 100, "CalibrationData layout must be free of implicit padding");

    inline std::optional<float> EncoderIndexAngle(const CalibrationData& data)
    {
        constexpr float pi = std::numbers::pi_v<float>;

        if (!std::isfinite(data.encoderIndexAngle) || data.encoderIndexAngle < -pi || data.encoderIndexAngle > pi)
            return std::nullopt;

        return data.encoderIndexAngle;
    }

    inline void ForgetEncoderIndexAngle(CalibrationData& data)
    {
        data.encoderIndexAngle = std::numeric_limits<float>::quiet_NaN();
    }

    inline uint16_t CurrentNoiseMilliAmpere(float noiseAmpere)
    {
        return static_cast<uint16_t>(std::clamp(noiseAmpere * 1000.0f + 0.5f, 0.0f, 65535.0f));
    }

    inline float CurrentNoiseAmpere(uint16_t noiseMilliAmpere)
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
        viscousFriction,
        frictionCoulomb
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
            case MotorParameter::viscousFriction:
                return data.frictionViscous;
            default:
                return data.frictionCoulomb;
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
            case MotorParameter::viscousFriction:
                data.frictionViscous = value;
                break;
            default:
                data.frictionCoulomb = value;
                break;
        }
    }
}
