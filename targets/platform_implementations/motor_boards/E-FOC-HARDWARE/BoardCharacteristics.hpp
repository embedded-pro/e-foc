#pragma once

#include <cstdint>

namespace application
{
    // E-FOC-HARDWARE custom motor control board.
    // Analog front-end values are identical to FRDM-MC-LVPMSM (same voltage divider ratio and
    // current sensor gain). Verified correct as initial bring-up defaults; update when schematic differs.
    struct BoardCharacteristics
    {
        static constexpr float voltageToVolts{ 18.433f };
        static constexpr float overvoltageThresholdVolts{ 58.0f };

        static constexpr float voltageToCurrent{ 5.0f };
        static constexpr float maxCurrentAmps{ 15.0f };
        static constexpr float overcurrentThresholdAmps{ 12.0f };

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

        static constexpr uint16_t OvercurrentThresholdCounts(float adcResolution)
        {
            return static_cast<uint16_t>((overcurrentThresholdAmps / maxCurrentAmps) * (adcResolution - 1.0f));
        }
    };

    static_assert(BoardCharacteristics::OvervoltageThresholdCounts(3.3f, 4096.0f) == 3904u, "E-FOC-HARDWARE overvoltage threshold mismatch");
    static_assert(BoardCharacteristics::OvercurrentThresholdCounts(4096.0f) == 3276u, "E-FOC-HARDWARE overcurrent threshold mismatch");
}
