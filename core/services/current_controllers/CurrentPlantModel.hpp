#pragma once

#include "core/services/current_controllers/CurrentController.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "numerical/math/Math.hpp"

namespace services
{
    struct CurrentPlantModel
    {
        float ad{ 1.0f };
        float bd{ 0.0f };

        static CurrentPlantModel FromParameters(const MotorModelParameters& parameters);
    };

    bool AreElectricalParametersValid(const MotorModelParameters& parameters);
    float NormalizationScale(foc::Volts busVoltage);
    float SamplePeriod(hal::Hertz samplingFrequency);
    float InductanceInHenry(foc::MilliHenry inductance);

    // SVM stays linear only inside the inscribed circle |Vdq| <= 1; a per-axis clamp would allow sqrt(2)
    ALWAYS_INLINE_HOT foc::RotatingFrame LimitToModulationCircle(const foc::RotatingFrame& voltages)
    {
        const auto squaredMagnitude = voltages.d * voltages.d + voltages.q * voltages.q;

        if (squaredMagnitude <= 1.0f)
            return voltages;

        const auto scale = 1.0f / math::Sqrt(squaredMagnitude);

        return { voltages.d * scale, voltages.q * scale };
    }
}
