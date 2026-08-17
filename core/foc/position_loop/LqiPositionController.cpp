#include "core/foc/position_loop/LqiPositionController.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    LqiPositionController::Design LqiPositionController::Inert()
    {
        return Design{ Design::GainMatrix{ { 0.0f, 0.0f, 0.0f } } };
    }

    std::optional<LqiPositionController::Design> LqiPositionController::Solve(const MechanicalModelParameters& parameters, const PositionLoopTunings& tunings)
    {
        if (!AreMechanicalParametersValid(parameters))
            return std::nullopt;

        const auto plant = PositionPlantModel::FromParameters(parameters);

        // The integral state is scaled by Ts as well, so it simply accumulates the deviation
        const Design::StateMatrix a{
            { 1.0f, 1.0f, 0.0f },
            { 0.0f, 1.0f, 1.0f },
            { 0.0f, 0.0f, plant.speedDecay }
        };
        const Design::InputMatrix b{ { 0.0f }, { 0.0f }, { 1.0f } };
        const Design::StateMatrix q{
            { WeightRatio(tunings.integralWeight, tunings.positionErrorWeight), 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, WeightRatio(tunings.speedErrorWeight, tunings.positionErrorWeight) }
        };
        const Design::InputWeightMatrix r{ NormalizedEffortWeight(tunings.bandwidth, parameters.samplingFrequency) };

        return Design::TryCreate(a, b, q, r);
    }

    bool LqiPositionController::IsDesignFeasible(const MechanicalModelParameters& parameters, const PositionLoopTunings& tunings)
    {
        return Solve(parameters, tunings).has_value();
    }

    void LqiPositionController::Configure(const MechanicalModelParameters& motorParameters)
    {
        parameters = motorParameters;
        Construct();
    }

    void LqiPositionController::SetTunings(const PositionLoopTunings& controllerTunings)
    {
        tunings = controllerTunings;
        Construct();
    }

    void LqiPositionController::Reset()
    {
        accumulatedDeviation = 0.0f;
    }

    OPTIMIZE_FOR_SPEED
    PositionOutput LqiPositionController::Compute(const PositionControlContext& context)
    {
        if (!designed)
            return { PositionOutputKind::currentReference, 0.0f };

        const auto deviation = -WrappedPositionError(context.reference, context.measured);
        const auto scaledSpeed = context.measuredSpeed.Value() * OuterSamplePeriod(parameters.samplingFrequency);

        accumulatedDeviation += deviation;

        const auto command = design.ComputeControl(Design::StateVector{ accumulatedDeviation, deviation, scaledSpeed });
        const auto unlimited = command.at(0, 0) * currentPerNormalizedInput;
        const auto limited = LimitToCurrentEnvelope(unlimited, parameters.maxCurrent);

        // Unwind the accumulation whenever the envelope saturates, so the integral cannot wind up
        if (limited.Value() != unlimited)
            accumulatedDeviation -= deviation;

        return { PositionOutputKind::currentReference, limited.Value() };
    }

    void LqiPositionController::Construct()
    {
        accumulatedDeviation = 0.0f;

        auto solved = Solve(parameters, tunings);

        // A rejected design leaves the loop inert rather than running unknown gains
        designed = solved.has_value();
        design = solved ? *solved : Inert();
        currentPerNormalizedInput = solved ? PositionPlantModel::FromParameters(parameters).currentPerNormalizedInput : 0.0f;
    }
}
