#pragma once

#include "core/foc/current_loop/CurrentController.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "numerical/math/Math.hpp"

namespace foc
{
    struct CurrentPlantModel
    {
        float ad{ 1.0f };
        float bd{ 0.0f };

        static CurrentPlantModel FromParameters(const MotorModelParameters& parameters);

        bool IsUsable() const;
    };

    bool AreElectricalParametersValid(const MotorModelParameters& parameters);
    float NormalizationScale(foc::Volts busVoltage);
    float SamplePeriod(hal::Hertz samplingFrequency);
    float InductanceInHenry(foc::MilliHenry inductance);

    class DecouplingFeedforward
    {
    public:
        void Configure(const MotorModelParameters& parameters);

        ALWAYS_INLINE_HOT foc::RotatingFrame Apply(const foc::RotatingFrame& voltages, const CurrentControlContext& context) const
        {
            const auto speed = context.electricalSpeed;

            const auto d = voltages.d - speed * couplingScale * context.measured.q;
            const auto q = voltages.q + speed * (couplingScale * context.measured.d + backEmfScale);

            return { d, q };
        }

    private:
        float couplingScale{ 0.0f };
        float backEmfScale{ 0.0f };
    };

    ALWAYS_INLINE_HOT foc::RotatingFrame LimitToModulationCircle(const foc::RotatingFrame& voltages)
    {
        const auto squaredMagnitude = voltages.d * voltages.d + voltages.q * voltages.q;

        const auto scale = 1.0f / math::Sqrt(squaredMagnitude > 1.0f ? squaredMagnitude : 1.0f);

        return { voltages.d * scale, voltages.q * scale };
    }
}
