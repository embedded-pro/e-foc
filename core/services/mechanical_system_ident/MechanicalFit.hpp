#pragma once

#include "core/services/mechanical_system_ident/InertiaSettling.hpp"
#include "core/services/mechanical_system_ident/MechanicalEstimatePolicy.hpp"
#include "core/services/mechanical_system_ident/ObservationQueue.hpp"
#include <cstddef>
#include <optional>

namespace services
{
    struct MechanicalEstimate
    {
        float friction{ 0.0f };
        float inertia{ 0.0f };
    };

    class MechanicalFit
    {
    public:
        void Restart(float forgettingFactor, std::size_t dwellSamples, std::size_t minimumRunSamples);
        void Stop();
        void Fit(const IdentificationObservation& observation);
        MechanicalEstimate Estimate() const;
        bool HasConverged() const;

        bool IsRunning() const
        {
            return rls.has_value();
        }

        bool IsFinishedAt(std::size_t sample) const
        {
            return finishAtSample != 0 && sample >= finishAtSample;
        }

    private:
        std::optional<MechanicalRls> rls;
        MechanicalRls::InputMatrix regressor;
        math::Matrix<float, 1, 1> torque;
        MechanicalExcitation excitation;
        InertiaSettling settling;
        std::size_t dwellSamples{ 1 };
        std::size_t minimumRunSamples{ 0 };
        volatile std::size_t finishAtSample{ 0 };
    };
}
