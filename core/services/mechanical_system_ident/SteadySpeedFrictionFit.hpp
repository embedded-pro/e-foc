#pragma once

#include "core/services/mechanical_system_ident/MechanicalEstimatePolicy.hpp"
#include <cmath>
#include <cstdint>

namespace services
{
    // Per constant-speed run, the mean speed and torque a viscous-friction slope is fitted from.
    class SteadySpeedFrictionFit
    {
    public:
        static constexpr float speedBand{ 0.03f };
        static constexpr uint32_t minimumRunObservations{ 16 };
        static constexpr uint32_t maximumRunObservations{ 256 };

        void Add(float speed, float torque, float forgettingFactor)
        {
            if (runObservations != 0 && std::abs(speed - RunSpeed()) > speedBand * std::abs(RunSpeed()))
                EndRun();

            runSpeed += speed;
            runTorque += torque;
            runDecay *= forgettingFactor;

            if (++runObservations == maximumRunObservations)
                EndRun();
        }

        void EndRun()
        {
            if (runObservations >= minimumRunObservations)
                Fold();

            runObservations = 0;
            runSpeed = 0.0f;
            runTorque = 0.0f;
            runDecay = 1.0f;
        }

        bool SpansDistinctSpeeds() const
        {
            const auto spread = 0.5f * mechanical_estimate::minimumSpeedSpan * std::abs(meanSpeed);

            return weight != 0.0f && speedVariance >= spread * spread * weight;
        }

        float SpeedVariance() const
        {
            return speedVariance;
        }

        float SpeedTorqueCovariance() const
        {
            return speedTorqueCovariance;
        }

    private:
        float RunSpeed() const
        {
            return runSpeed / static_cast<float>(runObservations);
        }

        void Fold()
        {
            const auto observations = static_cast<float>(runObservations);
            const auto speed = runSpeed / observations;
            const auto torque = runTorque / observations;

            weight = runDecay * weight + observations;

            const auto speedDeviation = speed - meanSpeed;
            meanSpeed += observations * speedDeviation / weight;
            meanTorque += observations * (torque - meanTorque) / weight;

            speedVariance = runDecay * speedVariance + observations * speedDeviation * (speed - meanSpeed);
            speedTorqueCovariance = runDecay * speedTorqueCovariance + observations * speedDeviation * (torque - meanTorque);
        }

        uint32_t runObservations{ 0 };
        float runSpeed{ 0.0f };
        float runTorque{ 0.0f };
        float runDecay{ 1.0f };

        float weight{ 0.0f };
        float meanSpeed{ 0.0f };
        float meanTorque{ 0.0f };
        float speedVariance{ 0.0f };
        float speedTorqueCovariance{ 0.0f };
    };
}
