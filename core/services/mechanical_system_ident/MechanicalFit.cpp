#include "core/services/mechanical_system_ident/MechanicalFit.hpp"
#include <algorithm>

namespace services
{
    namespace
    {
        constexpr float initialCovariance{ 1000.0f };
        constexpr float coveredWindowSamples{ 16.0f };
    }

    void MechanicalFit::Restart(float forgettingFactor, std::size_t dwellSamples, std::size_t minimumRunSamples)
    {
        rls.emplace(initialCovariance, forgettingFactor);
        settling.Restart(forgettingFactor);
        excitation.Restart();
        this->dwellSamples = dwellSamples;
        this->minimumRunSamples = minimumRunSamples;
        finishAtSample = 0;
    }

    void MechanicalFit::Stop()
    {
        rls.reset();
    }

    void MechanicalFit::Fit(const IdentificationObservation& observation)
    {
        if (IsFinishedAt(observation.sample))
            return;

        MechanicalRls::MakeRegressor(regressor, observation.acceleration, observation.speed);
        torque.at(0, 0) = observation.torque;

        const auto metrics = rls->Update(regressor, torque);
        excitation.Count(observation.acceleration, observation.speed);
        settling.Track(rls->Coefficients().at(1, 0), observation.windowSamples);

        const auto windowScale = std::max(1.0f, static_cast<float>(observation.windowSamples) / coveredWindowSamples);
        if (finishAtSample != 0 || observation.sample < minimumRunSamples || !settling.Settled() || !HasConvergedMechanics(metrics, excitation, settling.ForgettingFactor(), windowScale))
            return;

        finishAtSample = (observation.sample / dwellSamples + 1) * dwellSamples;
    }

    MechanicalEstimate MechanicalFit::Estimate() const
    {
        const auto& theta = rls->Coefficients();
        return MechanicalEstimate{ SettledFriction(theta.at(2, 0)), theta.at(1, 0) };
    }

    bool MechanicalFit::HasConverged() const
    {
        return finishAtSample != 0;
    }
}
