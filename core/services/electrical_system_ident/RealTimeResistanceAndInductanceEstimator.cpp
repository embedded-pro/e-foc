#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace services
{
    RealTimeResistanceAndInductanceEstimator::RealTimeResistanceAndInductanceEstimator(
        float forgettingFactor,
        hal::Hertz samplingFrequency)
        : rls{ 1000.0f, forgettingFactor }
        , samplingPeriod{ 1.0f / static_cast<float>(samplingFrequency.Value()) }
    {}

    // Voltage model: Vd = R*Id + L*(dId/dt - we*Iq)
    // Regressor: phi = [Id,  (dId/dt - we*Iq)]
    // Target: theta = [R, L]^T
    // Assumes non-salient motor (Ld = Lq). Uses Ld for seeding and reporting.
    void RealTimeResistanceAndInductanceEstimator::ComputeEstimate(
        foc::Volts vd,
        foc::Ampere id,
        foc::Ampere iq,
        foc::RadiansPerSecond electricalSpeed)
    {
        const float vdVal = vd.Value();
        const float idVal = id.Value();
        const float iqVal = iq.Value();
        const float weVal = electricalSpeed.Value();

        const float dIdDt = (idVal - previousId) / samplingPeriod;
        previousId = idVal;

        ElecRLS::InputMatrix regressor;
        regressor.at(0, 0) = idVal;
        regressor.at(1, 0) = dIdDt - weVal * iqVal;

        // Persistence-of-excitation gate: skip update when regressor is near zero
        const float phiNormSq = regressor.at(0, 0) * regressor.at(0, 0) + regressor.at(1, 0) * regressor.at(1, 0);
        if (phiNormSq >= persistenceOfExcitationThreshold)
        {
            math::Matrix<float, 1, 1> output;
            output.at(0, 0) = vdVal;
            rls.Update(regressor, output);
        }

        currentResistance = foc::Ohm{ rls.Coefficients().at(0, 0) };
        currentInductance = foc::MilliHenry{ rls.Coefficients().at(1, 0) * 1000.0f };
    }

    void RealTimeResistanceAndInductanceEstimator::Seed(foc::Ohm resistance, foc::MilliHenry inductance)
    {
        // RLS theta = [R (Ohm), L (Henry)]. Inductance stored externally in mH, converted here.
        ElecRLS::CoefficientsMatrix initial{};
        initial.at(0, 0) = resistance.Value();
        initial.at(1, 0) = inductance.Value() / 1000.0f;
        rls.SetCoefficients(initial);
        previousId = 0.0f;
    }

    void RealTimeResistanceAndInductanceEstimator::SetInitialEstimate(foc::Ohm resistance, foc::MilliHenry inductance)
    {
        currentResistance = resistance;
        currentInductance = inductance;
        Seed(resistance, inductance);
    }

    void RealTimeResistanceAndInductanceEstimator::Update(foc::Volts vd, foc::Ampere id, foc::Ampere iq, foc::RadiansPerSecond electricalSpeed)
    {
        ComputeEstimate(vd, id, iq, electricalSpeed);
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
