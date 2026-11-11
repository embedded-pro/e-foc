#include "core/services/electrical_system_ident/RealTimeFluxLinkageEstimator.hpp"
#include "numerical/math/Math.hpp"

namespace services
{
    namespace
    {
        constexpr float henryPerMilliHenry{ 1e-3f };
    }

    RealTimeFluxLinkageEstimator::RealTimeFluxLinkageEstimator(float forgettingFactor, hal::Hertz outerFrequency)
        : forgettingFactor{ forgettingFactor }
        , outerFrequency{ static_cast<float>(outerFrequency.Value()) }
    {}

    void RealTimeFluxLinkageEstimator::SetInitialEstimate(foc::Weber fluxLinkage)
    {
        seed = fluxLinkage.Value();
        published = seed;
        crossCorrelation = 0.0f;
        autoCorrelation = 0.0f;
        previousIqAtEnd = 0.0f;
        settled = false;
        updates = 0;
    }

    bool RealTimeFluxLinkageEstimator::Informative(const foc::QuadratureWindow& window, float speed) const
    {
        return !window.first && math::Abs(speed) * seed >= minimumBackEmfFraction * window.phaseVoltageRange.Value();
    }

    void RealTimeFluxLinkageEstimator::Update(const foc::QuadratureWindow& window)
    {
        const auto iqAtEnd = window.iqAtEnd.Value();
        const auto iqChange = iqAtEnd - previousIqAtEnd;
        previousIqAtEnd = iqAtEnd;

        const auto speed = window.meanElectricalSpeed.Value();
        if (!Informative(window, speed))
            return;

        const auto inductance = window.inductance.Value() * henryPerMilliHenry;
        const auto drop = window.resistance.Value() * window.meanIq.Value() + inductance * (iqChange * outerFrequency + speed * window.meanId.Value());

        crossCorrelation = forgettingFactor * crossCorrelation + speed * (window.meanAppliedVq.Value() - drop);
        autoCorrelation = forgettingFactor * autoCorrelation + speed * speed;

        if (updates != minimumUpdates)
            ++updates;

        Publish();
    }

    void RealTimeFluxLinkageEstimator::Publish()
    {
        if (updates < minimumUpdates)
            return;

        const auto estimate = crossCorrelation / autoCorrelation;
        if (!math::IsFinite(estimate) || estimate <= 0.0f)
            return;

        published = estimate;
        settled = true;
    }

    foc::Weber RealTimeFluxLinkageEstimator::CurrentFluxLinkage() const
    {
        return foc::Weber{ published };
    }

    bool RealTimeFluxLinkageEstimator::HasSettled() const
    {
        return settled;
    }
}
