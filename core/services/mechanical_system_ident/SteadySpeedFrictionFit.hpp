#pragma once

#include "core/services/mechanical_system_ident/MechanicalEstimatePolicy.hpp"
#include "numerical/estimators/online/RunningLinearFit.hpp"
#include <cmath>
#include <cstdint>

namespace services
{
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
            const auto spread = 0.5f * mechanical_estimate::minimumSpeedSpan * std::abs(line.MeanX());

            return line.Weight() != 0.0f && line.SumSquaredDeviationsX() >= spread * spread * line.Weight();
        }

        float SpeedVariance() const
        {
            return line.SumSquaredDeviationsX();
        }

        float SpeedTorqueCovariance() const
        {
            return line.SumCrossDeviations();
        }

    private:
        float RunSpeed() const
        {
            return runSpeed / static_cast<float>(runObservations);
        }

        void Fold()
        {
            const auto observations = static_cast<float>(runObservations);

            line.Decay(runDecay);
            line.Add(runSpeed / observations, runTorque / observations, observations);
        }

        uint32_t runObservations{ 0 };
        float runSpeed{ 0.0f };
        float runTorque{ 0.0f };
        float runDecay{ 1.0f };
        estimators::RunningLinearFit<float> line;
    };
}
