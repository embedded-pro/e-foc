#include "core/foc/speed_loop/LqiSpeedController.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"
#include "numerical/math/LinearTimeInvariant.hpp"
#include "numerical/math/Math.hpp"

namespace
{
    using SpeedPlant = math::LinearTimeInvariant<float, 1, 1, 1>;
}

namespace foc
{
    bool LqiSpeedController::IsDesignFeasible(const MechanicalModelParameters& parameters, const SpeedLoopTunings& tunings)
    {
        return Solve(parameters, tunings).has_value();
    }

    bool LqiSpeedController::Configure(const MechanicalModelParameters& motorParameters)
    {
        parameters = motorParameters;
        return Construct();
    }

    bool LqiSpeedController::SetTunings(const SpeedLoopTunings& controllerTunings)
    {
        tunings = controllerTunings;
        return Construct();
    }

    void LqiSpeedController::Reset()
    {
        lqi.Reset();
    }

    foc::Ampere LqiSpeedController::Compute(const SpeedControlContext& context)
    {
        const SpeedLqi::StateVector state{ context.measured.Value() };
        const SpeedLqi::OutputVector measured{ context.measured.Value() };
        const SpeedLqi::OutputVector reference{ context.reference.Value() };

        const auto control = lqi.ComputeControl(state, reference, measured).at(0, 0);

        if (math::Abs(control) > 1.0f)
        {
            const SpeedLqi::OutputVector antiWindup{ 2.0f * context.measured.Value() - context.reference.Value() };
            lqi.ComputeControl(state, antiWindup, measured);
        }

        return LimitToCurrentEnvelope(control * parameters.maxCurrent.Value(), parameters.maxCurrent);
    }

    LqiSpeedController::SpeedLqi LqiSpeedController::Inert()
    {
        return { SpeedLqi::GainStateMatrix{ 0.0f }, SpeedLqi::GainIntegralMatrix{ 0.0f }, 1.0f };
    }

    std::optional<LqiSpeedController::SpeedLqi> LqiSpeedController::Solve(const MechanicalModelParameters& parameters, const SpeedLoopTunings& tunings)
    {
        if (!AreMechanicalParametersValid(parameters))
            return std::nullopt;

        const auto plant = SpeedPlantModel::FromParameters(parameters);

        const math::SquareMatrix<float, 2> stateWeight{
            { tunings.speedErrorWeight, 0.0f },
            { 0.0f, tunings.integralWeight }
        };

        const auto stateMatrix = SpeedPlant::StateMatrix{ plant.ad };
        const auto normalizedInputGain = plant.bd * parameters.maxCurrent.Value();
        const auto inputMatrix = SpeedPlant::InputMatrix{ normalizedInputGain };
        const auto model = SpeedPlant::WithFullStateOutput(stateMatrix, inputMatrix);

        const math::SquareMatrix<float, 1> inputWeight{
            NormalizedEffortWeight(tunings.bandwidth, parameters.samplingFrequency) * normalizedInputGain * normalizedInputGain
        };

        return SpeedLqi::TryCreate(model, stateWeight, inputWeight, 1.0f);
    }

    bool LqiSpeedController::Construct()
    {
        auto solved = Solve(parameters, tunings);
        lqi = solved ? *solved : Inert();
        return solved.has_value();
    }
}
