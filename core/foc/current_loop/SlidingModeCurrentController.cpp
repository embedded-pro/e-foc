#include "core/foc/current_loop/SlidingModeCurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"
#include <cmath>

namespace foc
{
    bool SlidingModeCurrentController::Configure(const MotorModelParameters& motorParameters)
    {
        parameters = motorParameters;
        return Construct();
    }

    bool SlidingModeCurrentController::SetTunings(const CurrentLoopTunings& tunings)
    {
        bandwidth = tunings.bandwidth;
        switchingGain = tunings.switchingGain;
        boundaryLayer = tunings.boundaryLayer;
        return Construct();
    }

    void SlidingModeCurrentController::Reset() const
    {
        // The equivalent and switching terms are algebraic; there is no state to clear
    }

    SlidingModeCurrentController::ScalarSlidingMode SlidingModeCurrentController::Inert()
    {
        const auto stateMatrix = ScalarSlidingMode::PlantType::StateMatrix{ 0.0f };
        const auto inputMatrix = ScalarSlidingMode::PlantType::InputMatrix{ 1.0f };
        const auto plant = ScalarSlidingMode::PlantType::WithFullStateOutput(stateMatrix, inputMatrix);

        return { plant, ScalarSlidingMode::SurfaceMatrix{ 1.0f }, ScalarSlidingMode::InputVector{ 0.0f }, 1.0f };
    }

    bool SlidingModeCurrentController::Construct()
    {
        const auto plant = CurrentPlantModel::FromParameters(parameters);

        if (!AreElectricalParametersValid(parameters) || boundaryLayer <= 0.0f || !plant.IsUsable())
        {
            slidingMode = Inert();
            normalizationScale = 0.0f;
            equilibriumGain = 0.0f;
            decoupling = DecouplingFeedforward{};
            return false;
        }

        decoupling.Configure(parameters);

        normalizationScale = NormalizationScale(parameters.busVoltage);
        equilibriumGain = (1.0f - plant.ad) / plant.bd;

        const auto reachingPole = std::exp(-bandwidth * SamplePeriod(parameters.samplingFrequency));
        const auto stateMatrix = ScalarSlidingMode::PlantType::StateMatrix{ plant.ad - reachingPole };
        const auto inputMatrix = ScalarSlidingMode::PlantType::InputMatrix{ plant.bd };
        const auto plantNew = ScalarSlidingMode::PlantType::WithFullStateOutput(stateMatrix, inputMatrix);
        const auto surfaceMatrix = ScalarSlidingMode::SurfaceMatrix{ 1.0f };
        const auto inputVector = ScalarSlidingMode::InputVector{ switchingGain };

        slidingMode = ScalarSlidingMode{ plantNew, surfaceMatrix, inputVector, boundaryLayer };
        return true;
    }
}
