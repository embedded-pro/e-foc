#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include <cmath>

namespace services
{
    namespace
    {
        constexpr float initialCovariance{ 1e3f };
        constexpr float milliPerUnit{ 1e3f };
    }

    RealTimeResistanceAndInductanceEstimator::RealTimeResistanceAndInductanceEstimator(
        float forgettingFactor,
        hal::Hertz samplingFrequency)
        : forgettingFactor{ forgettingFactor }
        , rls{ std::in_place, initialCovariance, forgettingFactor }
        , samplingFrequency{ static_cast<float>(samplingFrequency.Value()) }
    {}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3", "fast-math")
#endif
    void RealTimeResistanceAndInductanceEstimator::ComputeEstimate(const foc::ElectricalWindow& window)
    {
        const auto idAtEnd = window.idAtEnd.Value();
        const auto idChange = idAtEnd - previousIdAtEnd;
        previousIdAtEnd = idAtEnd;

        if (!primed)
        {
            primed = true;
            return;
        }

        if (std::abs(window.meanId.Value()) < minimumDirectCurrent)
            return;

        ElecRLS::InputMatrix regressor;
        regressor.at(0, 0) = window.meanId.Value();
        regressor.at(1, 0) = (idChange * samplingFrequency - window.meanElectricalSpeedTimesIq) / milliPerUnit;

        math::Matrix<float, 1, 1> output;
        output.at(0, 0) = window.meanAppliedVd.Value();
        rls->Update(regressor, output);

        if (updates != minimumUpdates)
            ++updates;

        Publish();
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif

    void RealTimeResistanceAndInductanceEstimator::Publish()
    {
        if (updates < minimumUpdates)
            return;

        const auto resistance = rls->Coefficients().at(0, 0);
        const auto inductance = rls->Coefficients().at(1, 0);

        if (!foc::IsFiniteValue(resistance) || !foc::IsFiniteValue(inductance) || resistance <= 0.0f || inductance <= 0.0f)
            return;

        currentResistance = foc::Ohm{ resistance };
        currentInductance = foc::MilliHenry{ inductance };
    }

    void RealTimeResistanceAndInductanceEstimator::Seed(foc::Ohm resistance, foc::MilliHenry inductance)
    {
        rls.emplace(initialCovariance, forgettingFactor);

        ElecRLS::CoefficientsMatrix initial{};
        initial.at(0, 0) = resistance.Value();
        initial.at(1, 0) = inductance.Value();
        rls->SetCoefficients(initial);

        primed = false;
        updates = 0;
    }

    void RealTimeResistanceAndInductanceEstimator::SetInitialEstimate(foc::Ohm resistance, foc::MilliHenry inductance)
    {
        currentResistance = resistance;
        currentInductance = inductance;
        Seed(resistance, inductance);
    }

    void RealTimeResistanceAndInductanceEstimator::Update(const foc::ElectricalWindow& window)
    {
        ComputeEstimate(window);
    }

    foc::Ohm RealTimeResistanceAndInductanceEstimator::CurrentResistance() const
    {
        return currentResistance;
    }

    foc::MilliHenry RealTimeResistanceAndInductanceEstimator::CurrentInductance() const
    {
        return currentInductance;
    }
}
