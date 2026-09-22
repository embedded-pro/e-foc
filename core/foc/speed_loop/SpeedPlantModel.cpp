#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include <algorithm>
#include <hal/synchronous_interfaces/SynchronousPwm.hpp>

namespace foc
{
    bool AreMechanicalParametersValid(const MechanicalModelParameters& parameters)
    {
        return IsFinitePositive(parameters.inertia.Value()) &&
               IsFiniteNonNegative(parameters.viscousFriction.Value()) &&
               IsFinitePositive(parameters.torqueConstant.Value()) &&
               IsFinitePositive(parameters.maxCurrent.Value()) &&
               parameters.samplingFrequency.Value() > 0;
    }

    float OuterSamplePeriod(hal::Hertz samplingFrequency)
    {
        if (samplingFrequency.Value() == 0)
            return 0.0f;

        return 1.0f / static_cast<float>(samplingFrequency.Value());
    }

    float PlantInputGain(const MechanicalModelParameters& parameters)
    {
        return parameters.torqueConstant.Value() / parameters.inertia.Value();
    }

    SpeedPlantModel SpeedPlantModel::FromParameters(const MechanicalModelParameters& parameters)
    {
        const auto samplePeriod = OuterSamplePeriod(parameters.samplingFrequency);
        const auto ad = 1.0f - parameters.viscousFriction.Value() * samplePeriod / parameters.inertia.Value();

        return { ad, PlantInputGain(parameters) * samplePeriod };
    }

    float NormalizedEffortWeight(float bandwidth, hal::Hertz samplingFrequency)
    {
        const auto bandwidthPerSample = std::clamp(bandwidth * OuterSamplePeriod(samplingFrequency), 1e-3f, 0.5f);

        return 1.0f / (bandwidthPerSample * bandwidthPerSample);
    }
}
