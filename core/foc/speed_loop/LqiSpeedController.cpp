#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

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
    bool LqiSpeedController::Configure(const MechanicalModelParameters& motorParameters)
    {
        parameters = motorParameters;
        return Construct();
    }

    bool LqiSpeedController::SetTunings(const SpeedLoopTunings& tunings)
    {
        bandwidth = tunings.bandwidth;
        speedErrorWeight = tunings.speedErrorWeight;
        integralWeight = tunings.integralWeight;
        return Construct();
    }

    void LqiSpeedController::Reset()
    {
        lqi.Reset();
    }

    OPTIMIZE_FOR_SPEED
    foc::Ampere LqiSpeedController::Compute(const SpeedControlContext& context)
    {
        const SpeedLqi::StateVector state{ context.measured.Value() };
        const SpeedLqi::OutputVector measured{ context.measured.Value() };
        const SpeedLqi::OutputVector reference{ context.reference.Value() };

        const auto control = lqi.ComputeControl(state, reference, measured).at(0, 0);

        if (math::Abs(control) > 1.0f)
        {
            // Back-calculation anti-windup: feeding the negated error winds the
            // LQI integrator back toward zero when the output saturates.
            const SpeedLqi::OutputVector antiWindup{ 2.0f * context.measured.Value() - context.reference.Value() };
            lqi.ComputeControl(state, antiWindup, measured);
        }

        return LimitToCurrentEnvelope(control * parameters.maxCurrent.Value(), parameters.maxCurrent);
    }

    LqiSpeedController::SpeedLqi LqiSpeedController::Inert()
    {
        return { SpeedLqi::GainStateMatrix{ 0.0f }, SpeedLqi::GainIntegralMatrix{ 0.0f }, 1.0f };
    }

    bool LqiSpeedController::Construct()
    {
        if (!AreMechanicalParametersValid(parameters))
        {
            lqi = Inert();
            return false;
        }

        const auto plant = SpeedPlantModel::FromParameters(parameters);

        const math::SquareMatrix<float, 2> stateWeight{
            { speedErrorWeight, 0.0f },
            { 0.0f, integralWeight }
        };

        const auto stateMatrix = SpeedPlant::StateMatrix{ plant.ad };
        const auto normalizedInputGain = plant.bd * parameters.maxCurrent.Value();
        const auto inputMatrix = SpeedPlant::InputMatrix{ normalizedInputGain };
        const auto model = SpeedPlant::WithFullStateOutput(stateMatrix, inputMatrix);

        const math::SquareMatrix<float, 1> inputWeight{
            NormalizedEffortWeight(bandwidth, parameters.samplingFrequency) * normalizedInputGain * normalizedInputGain
        };

        lqi = SpeedLqi{ model, stateWeight, inputWeight, 1.0f };
        return true;
    }
}
