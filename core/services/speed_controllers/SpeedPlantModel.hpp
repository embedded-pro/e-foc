#pragma once

#include "core/services/speed_controllers/SpeedController.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>

namespace services
{
    struct SpeedPlantModel
    {
        float ad{ 1.0f };
        float bd{ 0.0f };

        static SpeedPlantModel FromParameters(const MechanicalModelParameters& parameters);
    };

    bool AreMechanicalParametersValid(const MechanicalModelParameters& parameters);
    float OuterSamplePeriod(hal::Hertz samplingFrequency);
    float PlantInputGain(const MechanicalModelParameters& parameters);

    ALWAYS_INLINE_HOT foc::Ampere LimitToCurrentEnvelope(float current, foc::Ampere maxCurrent)
    {
        const auto limit = maxCurrent.Value();

        return foc::Ampere{ std::clamp(current, -limit, limit) };
    }
}
