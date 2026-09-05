#include "core/foc/current_loop/CurrentPlantModel.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include "numerical/math/Math.hpp"
#include <numbers>

namespace
{
    constexpr float sqrt3 = std::numbers::sqrt3_v<float>;
    constexpr float henryPerMilliHenry = 0.001f;
}

namespace foc
{
    bool AreElectricalParametersValid(const MotorModelParameters& parameters)
    {
        return parameters.resistance.Value() > 0.0f &&
               parameters.inductance.Value() > 0.0f &&
               parameters.busVoltage.Value() > 0.0f &&
               parameters.samplingFrequency.Value() > 0;
    }

    float NormalizationScale(foc::Volts busVoltage)
    {
        return sqrt3 / busVoltage.Value();
    }

    float SamplePeriod(hal::Hertz samplingFrequency)
    {
        return 1.0f / static_cast<float>(samplingFrequency.Value());
    }

    float InductanceInHenry(foc::MilliHenry inductance)
    {
        return inductance.Value() * henryPerMilliHenry;
    }

    CurrentPlantModel CurrentPlantModel::FromParameters(const MotorModelParameters& parameters)
    {
        const auto resistance = parameters.resistance.Value();
        const auto inductance = InductanceInHenry(parameters.inductance);
        const auto samplePeriod = SamplePeriod(parameters.samplingFrequency);

        const auto ad = math::Exp(-resistance * samplePeriod / inductance);

        return { ad, (1.0f - ad) / resistance };
    }

    bool CurrentPlantModel::IsUsable() const
    {
        constexpr float minimumInputGain = 1e-9f;

        return IsFiniteValue(ad) && IsFiniteValue(bd) && bd > minimumInputGain && ad >= 0.0f && ad < 1.0f;
    }

    void DecouplingFeedforward::Configure(const MotorModelParameters& parameters)
    {
        couplingScale = 0.0f;
        backEmfScale = 0.0f;

        if (!AreElectricalParametersValid(parameters))
            return;

        const auto scale = NormalizationScale(parameters.busVoltage);
        couplingScale = InductanceInHenry(parameters.inductance) * scale;
        backEmfScale = parameters.fluxLinkage.Value() * scale;
    }
}
