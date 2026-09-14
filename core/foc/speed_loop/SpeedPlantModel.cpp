#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include "core/foc/interfaces/MotorModel.hpp"
#include <hal/synchronous_interfaces/SynchronousPwm.hpp>

namespace foc
{
    bool AreMechanicalParametersValid(const MechanicalModelParameters& parameters)
    {
        return parameters.inertia.Value() > 0.0f &&
               parameters.viscousFriction.Value() >= 0.0f &&
               parameters.torqueConstant.Value() > 0.0f &&
               parameters.maxCurrent.Value() > 0.0f &&
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
}
