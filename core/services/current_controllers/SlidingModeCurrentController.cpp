#include "core/services/current_controllers/SlidingModeCurrentController.hpp"
#include "core/services/current_controllers/CurrentPlantModel.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace services
{
    void SlidingModeCurrentController::Configure(const MotorModelParameters& motorParameters)
    {
        parameters = motorParameters;
        Construct();
    }

    void SlidingModeCurrentController::SetTunings(const CurrentControllerTunings& tunings)
    {
        switchingGain = tunings.switchingGain;
        boundaryLayer = tunings.boundaryLayer;
        Construct();
    }

    void SlidingModeCurrentController::Reset()
    {
        // The equivalent and switching terms are algebraic; there is no state to clear
    }

    OPTIMIZE_FOR_SPEED
    foc::RotatingFrame SlidingModeCurrentController::Compute(const CurrentControlContext& context)
    {
        return LimitToModulationCircle({ ComputeAxis(context.measured.d, context.reference.d),
            ComputeAxis(context.measured.q, context.reference.q) });
    }

    OPTIMIZE_FOR_SPEED
    float SlidingModeCurrentController::ComputeAxis(float measured, float reference)
    {
        const auto control = slidingMode.ComputeControl(ScalarSlidingMode::StateVector{ measured },
            ScalarSlidingMode::StateVector{ reference });

        return control.at(0, 0) * normalizationScale;
    }

    // Zero state and switching gain make the control law output zero until Configure supplies a plant
    SlidingModeCurrentController::ScalarSlidingMode SlidingModeCurrentController::Inert()
    {
        return { ScalarSlidingMode::PlantType::WithFullStateOutput(
                     ScalarSlidingMode::PlantType::StateMatrix{ 0.0f },
                     ScalarSlidingMode::PlantType::InputMatrix{ 1.0f }),
            ScalarSlidingMode::SurfaceMatrix{ 1.0f },
            ScalarSlidingMode::InputVector{ 0.0f },
            1.0f };
    }

    void SlidingModeCurrentController::Construct()
    {
        if (!AreElectricalParametersValid(parameters) || boundaryLayer <= 0.0f)
        {
            slidingMode = Inert();
            normalizationScale = 0.0f;
            return;
        }

        const auto plant = CurrentPlantModel::FromParameters(parameters);
        normalizationScale = NormalizationScale(parameters.busVoltage);

        // Both axes share the same decoupled RL plant, so a single stateless controller serves d and q
        slidingMode = ScalarSlidingMode{ ScalarSlidingMode::PlantType::WithFullStateOutput(
                                             ScalarSlidingMode::PlantType::StateMatrix{ plant.ad },
                                             ScalarSlidingMode::PlantType::InputMatrix{ plant.bd }),
            ScalarSlidingMode::SurfaceMatrix{ 1.0f },
            ScalarSlidingMode::InputVector{ switchingGain },
            boundaryLayer };
    }
}
