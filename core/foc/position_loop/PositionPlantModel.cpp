#include "core/foc/position_loop/PositionPlantModel.hpp"
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

        // omega * Ts advances by (Kt / J) * Ts^2 * Imax per unit of current; the normalised input
        // absorbs that factor, so converting a command back to Amperes is its reciprocal times Imax.
        const auto normalizedInputGain = (parameters.torqueConstant.Value() / inertia) * samplePeriod * samplePeriod * parameters.maxCurrent.Value();

        return PositionPlantModel{
            1.0f - (parameters.viscousFriction.Value() / inertia) * samplePeriod,
            normalizedInputGain > 0.0f ? parameters.maxCurrent.Value() / normalizedInputGain : 0.0f
        };
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
