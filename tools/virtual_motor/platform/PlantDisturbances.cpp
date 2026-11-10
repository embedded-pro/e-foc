#include "tools/virtual_motor/platform/PlantDisturbances.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>
#include <numbers>

namespace virtual_motor
{
    namespace
    {
        constexpr uint32_t sineFrequencyDivisor{ 10 };

        std::optional<port::RefusalReason> FiniteOrRefused(float value)
        {
            if (!math::IsFinite(value))
                return port::RefusalReason::outOfRange;
            return std::nullopt;
        }

        std::optional<port::RefusalReason> DeviationOrRefused(float value)
        {
            if (!math::IsFinite(value) || value < 0.0f)
                return port::RefusalReason::outOfRange;
            return std::nullopt;
        }

        uint32_t Differs(bool differs, uint8_t bit)
        {
            return differs ? uint32_t{ bit } : 0u;
        }
    }

    PlantDisturbances::PlantDisturbances(DisturbanceInputs& inputs, const DisturbanceBaseline& baseline, uint32_t controlRateHz)
        : inputs(inputs)
        , baseline(baseline)
        , controlRateHz(controlRateHz)
    {
        inForce.loadTorque = baseline.loadTorque.Value();
        inForce.currentNoise = baseline.currentNoise.Value();
        inForce.encoderNoise = baseline.encoderNoise.Value();
    }

    std::optional<port::RefusalReason> PlantDisturbances::SetLoadTorque(float newtonMeter)
    {
        if (auto refusal = FiniteOrRefused(newtonMeter))
            return refusal;

        inForce.loadTorque = newtonMeter;
        inputs.SetLoadTorque(foc::NewtonMeter{ newtonMeter });
        return std::nullopt;
    }

    std::optional<port::RefusalReason> PlantDisturbances::SetExternalTorque(const port::ExternalTorque& torque)
    {
        if (auto refusal = Validate(torque))
            return refusal;

        releaseExternalTorque = inForce.externalTorque.form != port::TorqueForm::none && torque.form == port::TorqueForm::none;
        inForce.externalTorque = torque;
        externalPeriods = 0;
        return std::nullopt;
    }

    std::optional<port::RefusalReason> PlantDisturbances::SetCurrentNoise(float ampere)
    {
        if (auto refusal = DeviationOrRefused(ampere))
            return refusal;

        inForce.currentNoise = ampere;
        inputs.SetCurrentNoise(foc::Ampere{ ampere });
        return std::nullopt;
    }

    std::optional<port::RefusalReason> PlantDisturbances::SetEncoderNoise(float radians)
    {
        if (auto refusal = DeviationOrRefused(radians))
            return refusal;

        inForce.encoderNoise = radians;
        inputs.SetEncoderNoise(foc::Radians{ radians });
        return std::nullopt;
    }

    void PlantDisturbances::ClearAll()
    {
        SetLoadTorque(baseline.loadTorque.Value());
        SetCurrentNoise(baseline.currentNoise.Value());
        SetEncoderNoise(baseline.encoderNoise.Value());
        SetExternalTorque(port::ExternalTorque{});
    }

    void PlantDisturbances::Apply()
    {
        if (releaseExternalTorque)
        {
            releaseExternalTorque = false;
            inputs.SetExternalTorque(foc::NewtonMeter{ 0.0f });
        }

        if (inForce.externalTorque.form == port::TorqueForm::none)
            return;

        const auto seconds = static_cast<float>(externalPeriods) / static_cast<float>(controlRateHz);
        inputs.SetExternalTorque(foc::NewtonMeter{ ExternalTorqueAt(seconds) });
        ++externalPeriods;
    }

    port::Changes PlantDisturbances::InForce() const
    {
        auto changes = inForce;
        changes.differs = static_cast<uint8_t>(Differs(inForce.loadTorque != baseline.loadTorque.Value(), port::differs::loadTorque) |
                                               Differs(inForce.externalTorque.form != port::TorqueForm::none, port::differs::externalTorque) |
                                               Differs(inForce.currentNoise != baseline.currentNoise.Value(), port::differs::currentNoise) |
                                               Differs(inForce.encoderNoise != baseline.encoderNoise.Value(), port::differs::encoderNoise));
        return changes;
    }

    std::optional<port::RefusalReason> PlantDisturbances::Validate(const port::ExternalTorque& torque) const
    {
        if (!std::ranges::all_of(torque.parameters, math::IsFinite<float>))
            return port::RefusalReason::outOfRange;

        const auto third = torque.parameters[2];
        switch (torque.form)
        {
            case port::TorqueForm::none:
            case port::TorqueForm::constant:
                return std::nullopt;
            case port::TorqueForm::step:
                return third >= 0.0f ? std::nullopt : std::optional{ port::RefusalReason::outOfRange };
            case port::TorqueForm::ramp:
                return third > 0.0f ? std::nullopt : std::optional{ port::RefusalReason::outOfRange };
            case port::TorqueForm::sine:
                return third > 0.0f && third <= static_cast<float>(controlRateHz / sineFrequencyDivisor) ? std::nullopt : std::optional{ port::RefusalReason::outOfRange };
            default:
                return port::RefusalReason::unknownForm;
        }
    }

    float PlantDisturbances::ExternalTorqueAt(float seconds) const
    {
        const auto& parameters = inForce.externalTorque.parameters;
        const auto first = parameters[0];
        const auto second = parameters[1];
        const auto third = parameters[2];
        switch (inForce.externalTorque.form)
        {
            case port::TorqueForm::step:
                return seconds < third ? first : second;
            case port::TorqueForm::ramp:
                return seconds >= third ? second : first + (second - first) * seconds / third;
            case port::TorqueForm::sine:
                return first + second * math::Sin(2.0f * std::numbers::pi_v<float> * third * seconds);
            default:
                return first;
        }
    }
}
