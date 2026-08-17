#include "core/foc/current_loop/DeadbeatCurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"
#include "numerical/controllers/implementations/DeadbeatControl.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace
{
    using OneStepDesign = controllers::DeadbeatControl<float, 1, 1, 1>;
    using TwoStepDesign = controllers::DeadbeatControl<float, 1, 1, 2>;

    template<typename Design>
    std::pair<float, float> ReferenceAndFeedbackGain(const foc::CurrentPlantModel& plant)
    {
        const Design design{ typename Design::StateMatrix{ plant.ad }, typename Design::InputMatrix{ plant.bd } };

        return { design.GetReferenceGain().at(0, 0), design.GetStateGain().at(0, 0) };
    }
}

namespace foc
{
    void DeadbeatCurrentController::Configure(const MotorModelParameters& motorParameters)
    {
        parameters = motorParameters;
        decoupling.Configure(motorParameters);
        ApplyGains();
    }

    void DeadbeatCurrentController::SetTunings(const CurrentLoopTunings& tunings)
    {
        twoStep = tunings.twoStepDeadbeat;
        ApplyGains();
    }

    void DeadbeatCurrentController::Reset() const
    {
        // Deadbeat control is a memoryless plant inversion; there is no state to clear
    }

    void DeadbeatCurrentController::ApplyGains()
    {
        referenceGain = 0.0f;
        feedbackGain = 0.0f;

        if (!AreElectricalParametersValid(parameters))
            return;

        const auto plant = CurrentPlantModel::FromParameters(parameters);
        const auto [reference, feedback] = twoStep ? ReferenceAndFeedbackGain<TwoStepDesign>(plant) : ReferenceAndFeedbackGain<OneStepDesign>(plant);
        const auto scale = NormalizationScale(parameters.busVoltage);

        referenceGain = reference * scale;
        feedbackGain = feedback * scale;
    }
}
