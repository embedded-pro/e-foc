#include "core/services/current_controllers/DeadbeatCurrentController.hpp"
#include "core/services/current_controllers/CurrentPlantModel.hpp"
#include "numerical/controllers/implementations/DeadbeatControl.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace
{
    using OneStepDesign = controllers::DeadbeatControl<float, 1, 1, 1>;
    using TwoStepDesign = controllers::DeadbeatControl<float, 1, 1, 2>;

    template<typename Design>
    std::pair<float, float> ReferenceAndFeedbackGain(const services::CurrentPlantModel& plant)
    {
        const Design design{ typename Design::StateMatrix{ plant.ad }, typename Design::InputMatrix{ plant.bd } };

        return { design.GetReferenceGain().at(0, 0), design.GetStateGain().at(0, 0) };
    }
}

namespace services
{
    void DeadbeatCurrentController::Configure(const MotorModelParameters& motorParameters)
    {
        parameters = motorParameters;
        ApplyGains();
    }

    void DeadbeatCurrentController::SetTunings(const CurrentControllerTunings& tunings)
    {
        twoStep = tunings.twoStepDeadbeat;
        ApplyGains();
    }

    void DeadbeatCurrentController::Reset()
    {
        // Deadbeat control is a memoryless plant inversion; there is no state to clear
    }

    OPTIMIZE_FOR_SPEED
    foc::RotatingFrame DeadbeatCurrentController::Compute(const CurrentControlContext& context) const
    {
        return LimitToModulationCircle({ referenceGain * context.reference.d - feedbackGain * context.measured.d,
            referenceGain * context.reference.q - feedbackGain * context.measured.q });
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
