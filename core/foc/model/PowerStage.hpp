#pragma once

namespace foc
{
    struct PowerStageConfig
    {
        float deadTimeSeconds{ 0.0f };
        float switchNodeCapacitanceFarad{ 0.0f };
        float onResistanceOhm{ 0.0f };
        float diodeForwardVolts{ 0.0f };
        float minimumPulseSeconds{ 0.0f };
    };

    // Mean voltage of one inverter leg over a centre-aligned PWM period, against the negative rail, for a leg
    // current positive out of the leg; see Part L of documentation/design/software-in-the-loop.md.
    float PowerStageLegVoltage(const PowerStageConfig& config, float duty, float current, float busVoltage, float period);
}
