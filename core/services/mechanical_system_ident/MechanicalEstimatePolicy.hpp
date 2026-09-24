#pragma once

#include "core/foc/math/FiniteGuard.hpp"
#include "numerical/estimators/online/RecursiveLeastSquares.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace services
{
    // Shared by the one-shot identification procedure and the online estimator so that an observation one
    // refuses is refused by the other, and an estimate one calls settled is settled for both; see
    // documentation/design/service-mechanical-ident.md.
    namespace mechanical_estimate
    {
        inline constexpr float minimumInertia{ 1e-9f };
        inline constexpr float maximumInertia{ 1.0f };
        inline constexpr float maximumFriction{ 1.0f };

        inline constexpr float innovationThreshold{ 1e-4f };
        inline constexpr float uncertaintyBudgetFactor{ 20.0f };
        inline constexpr float minimumUncertaintyThreshold{ 1e-3f };

        inline constexpr float minimumAcceleration{ 20.0f };
        inline constexpr float minimumSpeed{ 0.5f };
        inline constexpr float minimumSpeedSpan{ 0.25f };

        inline constexpr uint16_t minimumExcitedUpdates{ 64 };
    }

    using MechanicalRls = estimators::RecursiveLeastSquares<float, 3>;

    inline bool IsPlausibleMechanics(float inertia, float friction)
    {
        return foc::IsFiniteValue(inertia) && foc::IsFiniteValue(friction) &&
               inertia > mechanical_estimate::minimumInertia && inertia < mechanical_estimate::maximumInertia &&
               friction >= 0.0f && friction < mechanical_estimate::maximumFriction;
    }

    inline bool IsMechanicallyObservable(float speed)
    {
        return std::abs(speed) >= mechanical_estimate::minimumSpeed;
    }

    class MechanicalExcitation
    {
    public:
        void Restart()
        {
            *this = MechanicalExcitation{};
        }

        void Count(float acceleration, float speed)
        {
            if (std::abs(acceleration) >= mechanical_estimate::minimumAcceleration && accelerating != mechanical_estimate::minimumExcitedUpdates)
                ++accelerating;

            const auto magnitude = std::abs(speed);
            slowest = seen ? std::min(slowest, magnitude) : magnitude;
            fastest = seen ? std::max(fastest, magnitude) : magnitude;
            seen = true;
        }

        uint16_t AcceleratingObservations() const
        {
            return accelerating;
        }

        bool HasSpannedDistinctSpeeds() const
        {
            return seen && fastest - slowest >= mechanical_estimate::minimumSpeedSpan * fastest;
        }

    private:
        uint16_t accelerating{ 0 };
        float slowest{ 0.0f };
        float fastest{ 0.0f };
        bool seen{ false };
    };

    // An RLS that forgets cannot drive its covariance below a floor proportional to (1 - lambda), so a fixed
    // bound would be unreachable for a tracking estimator and trivially met by one that never forgets.
    inline float UncertaintyThresholdFor(float forgettingFactor)
    {
        return std::max(mechanical_estimate::uncertaintyBudgetFactor * (1.0f - forgettingFactor), mechanical_estimate::minimumUncertaintyThreshold);
    }

    inline bool HasConvergedMechanics(const MechanicalRls::EstimationMetrics& metrics, const MechanicalExcitation& excitation, float forgettingFactor)
    {
        return excitation.AcceleratingObservations() >= mechanical_estimate::minimumExcitedUpdates && excitation.HasSpannedDistinctSpeeds() &&
               MechanicalRls::EvaluateConvergence(metrics, mechanical_estimate::innovationThreshold, UncertaintyThresholdFor(forgettingFactor)) == estimators::State::converged;
    }
}
