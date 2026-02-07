#include "source/foc/implementations/WithRealTimeSpeedPidTune.hpp"
#include "numerical/estimators/RecursiveLeastSquares.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <numbers>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    OPTIMIZE_FOR_SPEED
    WithRealTimeSpeedPidTune::WithRealTimeSpeedPidTune(FieldOrientedControllerSpeedControl& focWithSpeedControl, FrictionAndInertiaEstimator& frictionAndInertiaEstimator, const Config& config)
        : focWithSpeedPid(focWithSpeedControl)
        , estimator(frictionAndInertiaEstimator)
        , innovationThreshold(config.innovationThreshold)
        , uncertityThreshold(config.uncertityThreshold)
        , vdc(config.vdc)
        , wc((static_cast<float>(config.speedSamplingFrequency.Value())) * 2.0f * std::numbers::pi_v<float>)
    {}

    OPTIMIZE_FOR_SPEED
    void WithRealTimeSpeedPidTune::Update(foc::PhaseCurrents currentPhases, RadiansPerSecond speed, Radians electricalAngle, foc::NewtonMeter targetTorque)
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
