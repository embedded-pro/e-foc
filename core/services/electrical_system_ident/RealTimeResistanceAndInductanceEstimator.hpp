#pragma once

#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "numerical/estimators/online/RecursiveLeastSquares.hpp"
#include <optional>

namespace services
{
    // Assumes a non-salient motor (Ld ≈ Lq, i.e., surface-mounted PMSM); an interior PMSM
    // would need a 3-parameter model separating Ld and Lq. Estimates start at zero until Seed().
    class RealTimeResistanceAndInductanceEstimator
        : public foc::OnlineElectricalEstimator
    {
    public:
        static constexpr float defaultForgettingFactor = 0.999f;
        static constexpr float minimumDirectCurrent = 0.05f;
        static constexpr uint16_t minimumUpdates = 200;

        explicit RealTimeResistanceAndInductanceEstimator(float forgettingFactor, hal::Hertz samplingFrequency);

        void Seed(foc::Ohm resistance, foc::MilliHenry inductance);

        // OnlineElectricalEstimator
        void SetInitialEstimate(foc::Ohm resistance, foc::MilliHenry inductance) override;

        void Update(const foc::ElectricalWindow& window) override;

        foc::Ohm CurrentResistance() const override;
        foc::MilliHenry CurrentInductance() const override;

    private:
        using ElecRLS = estimators::RecursiveLeastSquares<float, 2>;

        void ComputeEstimate(const foc::ElectricalWindow& window);
        void Publish();

        float forgettingFactor;
        std::optional<ElecRLS> rls;
        float samplingFrequency;
        float previousIdAtEnd{ 0.0f };
        bool primed{ false };
        uint16_t updates{ 0 };

        foc::Ohm currentResistance{ 0.0f };
        foc::MilliHenry currentInductance{ 0.0f };
    };
}
