#pragma once

#include <cstdint>

namespace application
{
    struct BoardCharacteristics
    {
        static constexpr float voltageToVolts{ 21.25f };
        static constexpr float overvoltageThresholdVolts{ 58.0f };

        static constexpr float voltageToCurrent{ 5.0f };
        static constexpr float ratedCurrentAmps{ 3.0f };
        // 80 % of the +8.25 A that a mid-rail biased 5 A/V sense spans on a 3.3 V ADC
        static constexpr float overcurrentThresholdAmps{ 6.6f };

        static constexpr float AdcToVoltsFactor(float adcReferenceVoltage, float adcResolution)
        {
            return (adcReferenceVoltage / adcResolution) * voltageToVolts;
        }

        static constexpr float AdcToAmpereSlope(float adcReferenceVoltage, float adcResolution)
        {
            return (adcReferenceVoltage / adcResolution) * voltageToCurrent;
        }

        static constexpr float AdcToAmpereOffset(float adcReferenceVoltage)
        {
            return -(adcReferenceVoltage / 2.0f) * voltageToCurrent;
        }

        static constexpr uint16_t OvervoltageThresholdCounts(float adcReferenceVoltage, float adcResolution)
        {
            return static_cast<uint16_t>((overvoltageThresholdVolts / (adcReferenceVoltage * voltageToVolts)) * (adcResolution - 1.0f));
        }

        // CUR_TOTAL is biased to mid-rail like the phase currents, so zero current reads half scale
        static constexpr uint16_t OvercurrentThresholdCounts(float adcReferenceVoltage, float adcResolution)
        {
            return static_cast<uint16_t>((0.5f + overcurrentThresholdAmps / (adcReferenceVoltage * voltageToCurrent)) * (adcResolution - 1.0f));
        }
    };

    static_assert(BoardCharacteristics::OvervoltageThresholdCounts(3.3f, 4096.0f) == 3386u, "E-FOC-HARDWARE overvoltage threshold mismatch");
    static_assert(BoardCharacteristics::OvercurrentThresholdCounts(3.3f, 4096.0f) == 3685u, "E-FOC-HARDWARE overcurrent threshold mismatch");
}
