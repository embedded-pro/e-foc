#pragma once

#include "numerical/estimators/online/RecursiveLeastSquares.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace services
{
    namespace mechanical_estimate
    {
        inline constexpr float minimumInertia{ 1e-9f };
        inline constexpr float maximumInertia{ 1.0f };
        inline constexpr float maximumFriction{ 1.0f };
        inline constexpr float frictionResolution{ 1e-4f };

        inline constexpr float innovationThreshold{ 1e-4f };
        inline constexpr float uncertaintyBudgetFactor{ 20.0f };
        inline constexpr float minimumUncertaintyThreshold{ 1e-3f };

        inline constexpr float minimumAcceleration{ 20.0f };
        inline constexpr float minimumSpeed{ 0.5f };
        inline constexpr float minimumSpeedSpan{ 0.25f };

        inline constexpr uint16_t minimumExcitedUpdates{ 64 };

        inline constexpr float agreementTolerance{ 0.2f };
    }

    using MechanicalRls = estimators::RecursiveLeastSquares<float, 3>;

    inline float SettledFriction(float friction)
    {
        return friction < 0.0f && friction > -mechanical_estimate::frictionResolution ? 0.0f : friction;
    }

    inline bool IsPlausibleMechanics(float inertia, float friction)
    {
        return math::IsFinite(inertia) && math::IsFinite(friction) &&
               inertia > mechanical_estimate::minimumInertia && inertia < mechanical_estimate::maximumInertia &&
               friction >= 0.0f && friction < mechanical_estimate::maximumFriction;
    }

    inline bool EstimatesAgree(float first, float second)
    {
        return std::abs(first - second) <= mechanical_estimate::agreementTolerance * std::max(std::abs(first), std::abs(second));
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
            CountAcceleration(acceleration);
            CountSpeed(speed);
        }

        void CountAcceleration(float acceleration)
        {
            if (std::abs(acceleration) >= mechanical_estimate::minimumAcceleration && accelerating != mechanical_estimate::minimumExcitedUpdates)
                ++accelerating;
        }

        void CountSpeed(float speed)
        {
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

    inline float UncertaintyThresholdFor(float forgettingFactor)
    {
        return std::max(mechanical_estimate::uncertaintyBudgetFactor * (1.0f - forgettingFactor), mechanical_estimate::minimumUncertaintyThreshold);
    }

    inline bool HasConvergedMechanics(const MechanicalRls::EstimationMetrics& metrics, const MechanicalExcitation& excitation, float forgettingFactor, float windowScale = 1.0f)
    {
        return static_cast<float>(excitation.AcceleratingObservations()) * windowScale >= mechanical_estimate::minimumExcitedUpdates && excitation.HasSpannedDistinctSpeeds() &&
               MechanicalRls::EvaluateConvergence(metrics, mechanical_estimate::innovationThreshold, UncertaintyThresholdFor(forgettingFactor) * windowScale) == estimators::State::converged;
    }
}
