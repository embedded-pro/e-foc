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

    struct ConductionWindow
    {
        float start{ 0.0f };
        float end{ 0.0f };
    };

    // Mean leg voltage over a PWM period, current positive out of the leg; see SIL design doc Part L.
    float PowerStageLegVoltage(const PowerStageConfig& config, float duty, float current, float busVoltage, float period);

    // Interval around the low-side pulse centre in which the leg current flows through the shunt.
    ConductionWindow LowSideConduction(const PowerStageConfig& config, float duty, float current, float period);
}
