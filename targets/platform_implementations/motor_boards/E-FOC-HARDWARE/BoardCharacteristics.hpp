#pragma once

#include <cstdint>

namespace application
{
    struct BoardCharacteristics
    {
        static constexpr float voltageToVolts{ 21.25f };
        static constexpr float overvoltageThresholdVolts{ 58.0f };
        // The bus also feeds the gate driver's 15 V rail and the launchpad; 18 V keeps that rail in regulation
        static constexpr float undervoltageThresholdVolts{ 18.0f };

        static constexpr float voltageToCurrent{ 5.0f };
        static constexpr float ratedCurrentAmps{ 3.0f };
        // 80 % of the +8.25 A that a mid-rail biased 5 A/V sense spans on a 3.3 V ADC
        static constexpr float overcurrentThresholdAmps{ 6.6f };
        static constexpr float maxCurrentSupportedAmps{ ratedCurrentAmps };
        static_assert(maxCurrentSupportedAmps < overcurrentThresholdAmps);
        // Above this duty the low-side window at the PWM centre is too short for a shunt to read.
        static constexpr uint32_t maxReadableDutyPercent{ 70 };
        // The FAN7888 bootstrap charges only while the low side conducts, so no leg stays at full duty.
        static constexpr uint32_t maxDutyPercent{ 95 };
        static constexpr uint32_t measuredDeadBandNanoseconds{ 500 };
        static constexpr uint32_t effectiveDeadTimeNanoseconds{ 70 };
        static constexpr float deadTimeKneeAmps{ 0.2f };

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

        static constexpr uint16_t UndervoltageThresholdCounts(float adcReferenceVoltage, float adcResolution)
        {
            return static_cast<uint16_t>((undervoltageThresholdVolts / (adcReferenceVoltage * voltageToVolts)) * (adcResolution - 1.0f));
        }

        // CUR_TOTAL is biased to mid-rail like the phase currents, so zero current reads half scale
        static constexpr uint16_t OvercurrentThresholdCounts(float adcReferenceVoltage, float adcResolution)
        {
            return static_cast<uint16_t>((0.5f + overcurrentThresholdAmps / (adcReferenceVoltage * voltageToCurrent)) * (adcResolution - 1.0f));
        }
    };

    static_assert(BoardCharacteristics::OvervoltageThresholdCounts(3.3f, 4096.0f) == 3386u, "E-FOC-HARDWARE overvoltage threshold mismatch");
    static_assert(BoardCharacteristics::OvercurrentThresholdCounts(3.3f, 4096.0f) == 3685u, "E-FOC-HARDWARE overcurrent threshold mismatch");
    static_assert(BoardCharacteristics::UndervoltageThresholdCounts(3.3f, 4096.0f) == 1051u, "E-FOC-HARDWARE undervoltage threshold mismatch");
}
