#include "core/foc/current_loop/DeadbeatCurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include "numerical/controllers/implementations/DeadbeatControl.hpp"
#include "numerical/math/Math.hpp"

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
    bool DeadbeatCurrentController::Configure(const MotorModelParameters& motorParameters)
    {
        parameters = motorParameters;
        const bool gainsOk = ApplyGains();
        const bool decouplingOk = decoupling.Configure(motorParameters);
        return gainsOk && decouplingOk;
    }

    bool DeadbeatCurrentController::SetTunings(const CurrentLoopTunings& tunings)
    {
        twoStep = tunings.twoStepDeadbeat;
        bandwidth = tunings.bandwidth;
        return ApplyGains();
    }

    void DeadbeatCurrentController::Reset()
    {
        lastInput = RotatingFrame{ 0.0f, 0.0f };
        expected = RotatingFrame{ 0.0f, 0.0f };
        disturbance = RotatingFrame{ 0.0f, 0.0f };
        primed = false;
    }

    bool DeadbeatCurrentController::ApplyGains()
    {
        referenceGain = 0.0f;
        feedbackGain = 0.0f;
        plantStateGain = 1.0f;
        plantInputGain = 0.0f;
        disturbanceGain = 0.0f;
        delayed = false;

        if (!AreElectricalParametersValid(parameters))
            return false;

        const auto plant = CurrentPlantModel::FromParameters(parameters);

        if (!plant.IsUsable())
            return false;

        const auto [reference, feedback] = twoStep ? ReferenceAndFeedbackGain<TwoStepDesign>(plant) : ReferenceAndFeedbackGain<OneStepDesign>(plant);
        const auto scale = NormalizationScale(parameters.busVoltage);

        referenceGain = reference * scale;
        feedbackGain = feedback * scale;
        plantStateGain = plant.ad;
        plantInputGain = plant.bd / scale;
        delayed = parameters.dutyUpdateDelayPeriods != 0;

        if (IsFinitePositive(bandwidth))
            disturbanceGain = (1.0f - math::Exp(-bandwidth * SamplePeriod(parameters.samplingFrequency))) / plantInputGain;

        return true;
    }
}
