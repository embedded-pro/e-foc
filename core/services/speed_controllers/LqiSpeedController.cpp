#include "core/services/speed_controllers/LqiSpeedController.hpp"
#include "core/services/speed_controllers/SpeedPlantModel.hpp"
#include "numerical/math/LinearTimeInvariant.hpp"
#include "numerical/math/Math.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace
{
    using SpeedPlant = math::LinearTimeInvariant<float, 1, 1, 1>;
}

namespace services
{
    void LqiSpeedController::Configure(const MechanicalModelParameters& motorParameters)
    {
        parameters = motorParameters;
        Construct();
    }

    void LqiSpeedController::SetTunings(const SpeedControllerTunings& tunings)
    {
        speedErrorWeight = tunings.speedErrorWeight;
        integralWeight = tunings.integralWeight;
        Construct();
    }

    void LqiSpeedController::Reset()
    {
        lqi.Reset();
        integratorHalted = false;
    }

    OPTIMIZE_FOR_SPEED
    foc::Ampere LqiSpeedController::Compute(const SpeedControlContext& context)
    {
        const SpeedLqi::StateVector state{ context.measured.Value() };
        const SpeedLqi::OutputVector measured{ context.measured.Value() };

        // Conditional integration: a zero error while saturated freezes the integral state
        const SpeedLqi::OutputVector reference{ integratorHalted ? context.measured.Value() : context.reference.Value() };

        const auto control = lqi.ComputeControl(state, reference, measured).at(0, 0);
        integratorHalted = math::Abs(control) > 1.0f;

        return LimitToCurrentEnvelope(control * parameters.maxCurrent.Value(), parameters.maxCurrent);
    }

    // Zero gains keep the control law at zero until Configure supplies a plant
    LqiSpeedController::SpeedLqi LqiSpeedController::Inert()
    {
        return { SpeedLqi::GainStateMatrix{ 0.0f }, SpeedLqi::GainIntegralMatrix{ 0.0f }, 1.0f };
    }

    void LqiSpeedController::Construct()
    {
        if (!AreMechanicalParametersValid(parameters))
        {
            lqi = Inert();
            return;
        }

        const auto plant = SpeedPlantModel::FromParameters(parameters);

        const math::SquareMatrix<float, 2> stateWeight{
            { speedErrorWeight, 0.0f },
            { 0.0f, integralWeight }
        };
        const math::SquareMatrix<float, 1> inputWeight{ 1.0f };

        // Per-unit current and unit-sample integration leave the gain unchanged but keep the DARE iteration inside float precision
        lqi = SpeedLqi{ SpeedPlant::WithFullStateOutput(SpeedPlant::StateMatrix{ plant.ad },
                            SpeedPlant::InputMatrix{ plant.bd * parameters.maxCurrent.Value() }),
            stateWeight,
            inputWeight,
            1.0f };
    }
}
