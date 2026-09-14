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

        inline constexpr float minimumAcceleration{ 1.0f };
        inline constexpr float minimumSpeed{ 0.5f };

        inline constexpr uint16_t minimumExcitedUpdates{ 64 };
    }

    using MechanicalRls = estimators::RecursiveLeastSquares<float, 3>;

    inline bool IsPlausibleMechanics(float inertia, float friction)
    {
        return foc::IsFiniteValue(inertia) && foc::IsFiniteValue(friction) &&
               inertia > mechanical_estimate::minimumInertia && inertia < mechanical_estimate::maximumInertia &&
               friction >= 0.0f && friction < mechanical_estimate::maximumFriction;
    }

    // Rotation alone leaves the intercept and the speed column collinear and the acceleration column at zero,
    // so a constant speed identifies neither inertia nor viscous friction while still inflating the covariance.
    inline bool IsMechanicallyExciting(float acceleration, float speed)
    {
        return std::abs(acceleration) >= mechanical_estimate::minimumAcceleration &&
               std::abs(speed) >= mechanical_estimate::minimumSpeed;
    }

    // An RLS that forgets cannot drive its covariance below a floor proportional to (1 - lambda), so a fixed
    // bound would be unreachable for a tracking estimator and trivially met by one that never forgets.
    inline float UncertaintyThresholdFor(float forgettingFactor)
    {
        return std::max(mechanical_estimate::uncertaintyBudgetFactor * (1.0f - forgettingFactor), mechanical_estimate::minimumUncertaintyThreshold);
    }

    inline bool HasConvergedMechanics(const MechanicalRls::EstimationMetrics& metrics, uint16_t excitedUpdates, float forgettingFactor)
    {
        return excitedUpdates >= mechanical_estimate::minimumExcitedUpdates &&
               MechanicalRls::EvaluateConvergence(metrics, mechanical_estimate::innovationThreshold, UncertaintyThresholdFor(forgettingFactor)) == estimators::State::converged;
    }
}
