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
        , estimator{ std::in_place, initialCovariance, forgettingFactor }
        , samplingFrequency{ static_cast<float>(samplingFrequency.Value()) }
    {}

    void RealTimeResistanceAndInductanceEstimator::ComputeEstimate(const foc::ElectricalWindow& window)
    {
        const auto idAtEnd = window.idAtEnd.Value();
        const auto idChange = idAtEnd - previousIdAtEnd;
        previousIdAtEnd = idAtEnd;

        const auto reference = window.directCurrentReference.Value();
        const auto referenceChange = reference - previousReference;
        previousReference = reference;

        if (!primed)
        {
            primed = true;
            return;
        }

        if (std::abs(reference) < minimumDirectCurrent)
            return;

        Estimator::InputMatrix regressor;
        regressor.at(0, 0) = window.meanId.Value();
        regressor.at(1, 0) = (idChange * samplingFrequency - window.meanElectricalSpeedTimesIq) / milliPerUnit;

        Estimator::InputMatrix instrument;
        instrument.at(0, 0) = reference;
        instrument.at(1, 0) = referenceChange * samplingFrequency / milliPerUnit;

        math::Matrix<float, 1, 1> output;
        output.at(0, 0) = window.meanAppliedVd.Value();
        estimator->Update(regressor, instrument, output);

        if (updates != minimumUpdates)
            ++updates;

        Publish();
    }

    void RealTimeResistanceAndInductanceEstimator::Publish()
    {
        if (updates < minimumUpdates)
            return;

        const auto resistance = estimator->Coefficients().at(0, 0);
        const auto inductance = estimator->Coefficients().at(1, 0);

        if (!foc::IsFiniteValue(resistance) || !foc::IsFiniteValue(inductance) || resistance <= 0.0f || inductance <= 0.0f)
            return;

        currentResistance = foc::Ohm{ resistance };
        currentInductance = foc::MilliHenry{ inductance };
    }

    void RealTimeResistanceAndInductanceEstimator::Seed(foc::Ohm resistance, foc::MilliHenry inductance)
    {
        estimator.emplace(initialCovariance, forgettingFactor);

        Estimator::CoefficientsMatrix initial{};
        initial.at(0, 0) = resistance.Value();
        initial.at(1, 0) = inductance.Value();
        estimator->SetCoefficients(initial);

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
