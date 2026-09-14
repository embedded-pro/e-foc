#include "core/foc/position_loop/PositionPlantModel.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include <algorithm>

namespace foc
{
    namespace
    {
        constexpr float minimumWeightRatio = 1e-3f;
        constexpr float maximumWeightRatio = 1e3f;
    }

    PositionPlantModel PositionPlantModel::FromParameters(const MechanicalModelParameters& parameters)
    {
        const auto samplePeriod = OuterSamplePeriod(parameters.samplingFrequency);
        const auto inertia = parameters.inertia.Value();

        const auto normalizedInputGain = (parameters.torqueConstant.Value() / inertia) * samplePeriod * samplePeriod * parameters.maxCurrent.Value();
        const auto speedDecay = 1.0f - (parameters.viscousFriction.Value() / inertia) * samplePeriod;
        const auto currentPerNormalizedInput = normalizedInputGain > 0.0f ? parameters.maxCurrent.Value() / normalizedInputGain : 0.0f;

        return PositionPlantModel{ speedDecay, currentPerNormalizedInput };
    }

    float NormalizedEffortWeight(float bandwidth, hal::Hertz samplingFrequency)
    {
        const auto bandwidthPerSample = std::clamp(bandwidth * OuterSamplePeriod(samplingFrequency), 1e-3f, 0.5f);

        return 1.0f / (bandwidthPerSample * bandwidthPerSample);
    }

    float WeightRatio(float weight, float positionErrorWeight)
    {
        const auto anchor = positionErrorWeight > 0.0f ? positionErrorWeight : 1.0f;

        return std::clamp(weight / anchor, minimumWeightRatio, maximumWeightRatio);
    }
}
