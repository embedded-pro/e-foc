#pragma once

#include "foc/interfaces/FieldOrientedController.hpp"
#include "foc/interfaces/Units.hpp"
#include "numerical/estimators/RecursiveLeastSquares.hpp"

namespace foc
{
    template<typename FocWithSpeedControl, typename FrictionAndInertiaEstimator>
    class RealTimeSpeedPidTune
    {
    public:
        struct Config
        {
            float innovationThreshold{ 0.001f };
            float uncertityThreshold{ 0.1f };
            Volts vdc;
            hal::Hertz speedSamplingFrequency;
        };

        RealTimeSpeedPidTune(const Config& config);

        void Update(foc::PhaseCurrents currentPhases, RadiansPerSecond speed, Radians electricalAngle, foc::NewtonMeter targetTorque);

    protected:
        FocWithSpeedControl focWithSpeedPid;
        FrictionAndInertiaEstimator estimator;

    private:
        foc::SpeedTunings speedTunings;
        float innovationThreshold{ 0.001f };
        float uncertityThreshold{ 0.1f };
        Volts vdc;
        float wc;
    };

    // Implementation

    template<typename FocWithSpeedControl, typename FrictionAndInertiaEstimator>
    RealTimeSpeedPidTune<FocWithSpeedControl, FrictionAndInertiaEstimator>::RealTimeSpeedPidTune(const Config& config)
        : focWithSpeedPid(config.vdc, config.speedSamplingFrequency)
        , estimator(config.innovationThreshold, config.speedSamplingFrequency)
        , innovationThreshold(config.innovationThreshold)
        , uncertityThreshold(config.uncertityThreshold)
        , vdc(config.vdc)
        , wc((static_cast<float>(config.speedSamplingFrequency.Value())) * 2.0f * std::numbers::pi_v<float>){};

    template<typename FocWithSpeedControl, typename FrictionAndInertiaEstimator>
    void RealTimeSpeedPidTune<FocWithSpeedControl, FrictionAndInertiaEstimator>::Update(foc::PhaseCurrents currentPhases, RadiansPerSecond speed, Radians electricalAngle, foc::NewtonMeter targetTorque)
    {
        auto result = estimator.Update(currentPhases, speed, electricalAngle, targetTorque);

        if (estimators::RecursiveLeastSquares<float, 3>::EvaluateConvergence(result.metrics, innovationThreshold, uncertityThreshold) == estimators::State::converged)
        {
            speedTunings.kp = wc * result.inertia.Value();
            speedTunings.ki = wc * result.friction.Value();
            speedTunings.kd = 0.0f;

            focWithSpeedPid.SetSpeedTunings(vdc, speedTunings);
        }
    }
}
