#include "core/foc/position_loop/LqrPositionController.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    LqrPositionController::Design LqrPositionController::Inert()
    {
        return Design{ Design::GainMatrix{ { 0.0f, 0.0f } } };
    }

    std::optional<LqrPositionController::Design> LqrPositionController::Solve(const MechanicalModelParameters& parameters, const PositionLoopTunings& tunings)
    {
        if (!AreMechanicalParametersValid(parameters))
            return std::nullopt;

        const auto plant = PositionPlantModel::FromParameters(parameters);

        const Design::StateMatrix a{
            { 1.0f, 1.0f },
            { 0.0f, plant.speedDecay }
        };
        const Design::InputMatrix b{ { 0.0f }, { 1.0f } };
        const Design::StateMatrix q{
            { 1.0f, 0.0f },
            { 0.0f, WeightRatio(tunings.speedErrorWeight, tunings.positionErrorWeight) }
        };
        const Design::InputWeightMatrix r{ NormalizedEffortWeight(tunings.bandwidth, parameters.samplingFrequency) };

        // TryCreate keeps a non-convergent Riccati solve from aborting the firmware mid-operation
        return Design::TryCreate(a, b, q, r);
    }

    void LqrPositionController::Reset() const
    {
    }

    OPTIMIZE_FOR_SPEED
    PositionOutput LqrPositionController::Compute(const PositionControlContext& context)
    {
        const auto deviation = -WrappedPositionError(context.reference, context.measured);
        const auto scaledSpeed = context.measuredSpeed.Value() * samplePeriod;
        const auto command = design.ComputeControl(Design::StateVector{ deviation, scaledSpeed });

        return { PositionOutputKind::currentReference,
            LimitToCurrentEnvelope(command.at(0, 0) * currentPerNormalizedInput, parameters.maxCurrent).Value() };
    }

}
