#pragma once

#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "numerical/estimators/online/RecursiveLeastSquares.hpp"

namespace services
{
    // Real-time RLS estimator for phase resistance and d-axis inductance.
    // Assumes a non-salient motor (Ld ≈ Lq, i.e., surface-mounted PMSM).
    // For interior PMSMs, a 3-parameter model separating Ld and Lq would be required.
    // Seeds and reports using Ld. On construction, the RLS is seeded to zero; call
    // Seed() to warm-start from calibration data before enabling.
    class RealTimeResistanceAndInductanceEstimator
        : public foc::OnlineElectricalEstimator
    {
    public:
        static constexpr float defaultForgettingFactor = 0.998f;
        static constexpr float persistenceOfExcitationThreshold = 1e-6f;

        explicit RealTimeResistanceAndInductanceEstimator(float forgettingFactor, hal::Hertz samplingFrequency);

        void Seed(foc::Ohm resistance, foc::MilliHenry inductance);

        // OnlineElectricalEstimator
        void SetInitialEstimate(foc::Ohm resistance, foc::MilliHenry inductance) override;

        void Update(foc::Volts vd, foc::Ampere id, foc::Ampere iq, foc::RadiansPerSecond electricalSpeed) override;

        foc::Ohm CurrentResistance() const override;
        foc::MilliHenry CurrentInductance() const override;

    private:
        using ElecRLS = estimators::RecursiveLeastSquares<float, 2>;

        void ComputeEstimate(foc::Volts vd, foc::Ampere id, foc::Ampere iq, foc::RadiansPerSecond electricalSpeed);

        ElecRLS rls;
        float samplingPeriod;
        float previousId{ 0.0f };

        foc::Ohm currentResistance{ 0.0f };
        foc::MilliHenry currentInductance{ 0.0f };
    };
}
