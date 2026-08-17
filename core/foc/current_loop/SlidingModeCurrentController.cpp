#include "core/foc/current_loop/SlidingModeCurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    void SlidingModeCurrentController::Configure(const MotorModelParameters& motorParameters)
    {
        parameters = motorParameters;
        Construct();
    }

    void SlidingModeCurrentController::SetTunings(const CurrentLoopTunings& tunings)
    {
        switchingGain = tunings.switchingGain;
        boundaryLayer = tunings.boundaryLayer;
        Construct();
    }

    void SlidingModeCurrentController::Reset() const
    {
        // The equivalent and switching terms are algebraic; there is no state to clear
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
            equilibriumGain = 0.0f;
            return;
        }

        const auto plant = CurrentPlantModel::FromParameters(parameters);
        normalizationScale = NormalizationScale(parameters.busVoltage);
        equilibriumGain = (1.0f - plant.ad) / plant.bd;

        // Both axes share the same decoupled RL plant, so a single stateless controller serves d and q
        slidingMode = ScalarSlidingMode{ ScalarSlidingMode::PlantType::WithFullStateOutput(
                                             ScalarSlidingMode::PlantType::StateMatrix{ plant.ad },
                                             ScalarSlidingMode::PlantType::InputMatrix{ plant.bd }),
            ScalarSlidingMode::SurfaceMatrix{ 1.0f },
            ScalarSlidingMode::InputVector{ switchingGain },
            boundaryLayer };
    }
}
