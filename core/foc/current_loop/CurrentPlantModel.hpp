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

    // Cancels cross-axis coupling and back-EMF so the controller sees the decoupled per-axis RL
    // plant its gain design assumes; see documentation/theory/current-loop-controllers.md A1.
    class DecouplingFeedforward
    {
    public:
        void Configure(const MotorModelParameters& parameters);

        ALWAYS_INLINE_HOT foc::RotatingFrame Apply(const foc::RotatingFrame& voltages, const CurrentControlContext& context) const
        {
            const auto speed = context.electricalSpeed;

            return { voltages.d - speed * couplingScale * context.measured.q,
                voltages.q + speed * (couplingScale * context.measured.d + backEmfScale) };
        }

    private:
        float couplingScale{ 0.0f };
        float backEmfScale{ 0.0f };
    };

    // SVM stays linear only inside the inscribed circle |Vdq| <= 1; a per-axis clamp would allow sqrt(2)
    ALWAYS_INLINE_HOT foc::RotatingFrame LimitToModulationCircle(const foc::RotatingFrame& voltages)
    {
        const auto squaredMagnitude = voltages.d * voltages.d + voltages.q * voltages.q;

        if (squaredMagnitude <= 1.0f)
            return voltages;

        const auto scale = 1.0f / math::Sqrt(squaredMagnitude);

        return { voltages.d * scale, voltages.q * scale };
    }
}
