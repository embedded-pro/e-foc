#include "numerical/estimators/Estimator.hpp"
#include "source/foc/implementations/WithRealTimeSpeedPidTune.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    struct MockFocWithSpeedControl
    {
        MockFocWithSpeedControl(foc::Volts vdc, hal::Hertz frequency)
            : vdc(vdc)
            , frequency(frequency)
        {}

        void SetSpeedTunings(foc::Volts v, const foc::SpeedTunings& tunings)
        {
            lastVdc = v;
            lastTunings = tunings;
            tuningsSetCount++;
        }

        foc::Volts vdc;
        hal::Hertz frequency;
        foc::Volts lastVdc{ 0.0f };
        foc::SpeedTunings lastTunings{};
        int tuningsSetCount = 0;
    };

    struct MockEstimatorResult
    {
        foc::NewtonMeterSecondPerRadian inertia;
        foc::NewtonMeterSecondSquared friction;
        estimators::OnlineEstimator<float, 3>::EstimationMetrics metrics;
    };

    struct MockFrictionAndInertiaEstimator
    {
        MockFrictionAndInertiaEstimator(float forgettingFactor, hal::Hertz frequency)
            : forgettingFactor(forgettingFactor)
            , frequency(frequency)
        {
            result.inertia = foc::NewtonMeterSecondPerRadian{ 0.01f };
            result.friction = foc::NewtonMeterSecondSquared{ 0.001f };
            result.metrics.innovation = 0.1f;
            result.metrics.residual = 0.05f;
            result.metrics.uncertainty = 0.2f;
        }

        MockEstimatorResult Update(foc::PhaseCurrents currents, foc::RadiansPerSecond speed,
            foc::Radians angle, foc::NewtonMeter torque)
        {
            updateCalled = true;
            lastCurrents = currents;
            lastSpeed = speed;
            lastAngle = angle;
            lastTorque = torque;
            return result;
        }

        float forgettingFactor;
        hal::Hertz frequency;
        MockEstimatorResult result;
        bool updateCalled = false;
        foc::PhaseCurrents lastCurrents{};
        foc::RadiansPerSecond lastSpeed{ 0.0f };
        foc::Radians lastAngle{ 0.0f };
        foc::NewtonMeter lastTorque{ 0.0f };
    };

    class AccessibleRealTimeSpeedPidTune
        : public foc::RealTimeSpeedPidTune<MockFocWithSpeedControl, MockFrictionAndInertiaEstimator>
    {
    public:
        using Base = foc::RealTimeSpeedPidTune<MockFocWithSpeedControl, MockFrictionAndInertiaEstimator>;
        using Base::Base;

        MockFocWithSpeedControl& GetFoc()
        {
            return this->focWithSpeedPid;
        }

        MockFrictionAndInertiaEstimator& GetEstimator()
        {
            return this->estimator;
        }
    };

    using TestRealTimeSpeedPidTuneImpl = AccessibleRealTimeSpeedPidTune;

    class TestRealTimeSpeedPidTune
        : public ::testing::Test
    {
    public:
        void SetUp() override
        {
            config.vdc = foc::Volts{ 24.0f };
            config.speedSamplingFrequency = hal::Hertz{ 1000 };
            config.innovationThreshold = 0.001f;
            config.uncertityThreshold = 0.1f;
        }

        TestRealTimeSpeedPidTuneImpl::Config config;
    };
}

TEST_F(TestRealTimeSpeedPidTune, estimator_update_is_called_with_correct_parameters)
{
    TestRealTimeSpeedPidTuneImpl tuner(config);

    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::RadiansPerSecond speed{ 100.0f };
    foc::Radians angle{ 0.5f };
    foc::NewtonMeter torque{ 0.2f };

    tuner.Update(currents, speed, angle, torque);

    auto& estimator = tuner.GetEstimator();
    EXPECT_TRUE(estimator.updateCalled);
    EXPECT_EQ(estimator.lastCurrents.a.Value(), 1.0f);
    EXPECT_EQ(estimator.lastCurrents.b.Value(), -0.5f);
    EXPECT_EQ(estimator.lastCurrents.c.Value(), -0.5f);
    EXPECT_EQ(estimator.lastSpeed.Value(), 100.0f);
    EXPECT_EQ(estimator.lastAngle.Value(), 0.5f);
    EXPECT_EQ(estimator.lastTorque.Value(), 0.2f);
}

TEST_F(TestRealTimeSpeedPidTune, tunings_not_set_when_not_converged)
{
    config.innovationThreshold = 0.001f;
    config.uncertityThreshold = 0.05f;

    TestRealTimeSpeedPidTuneImpl tuner(config);

    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::RadiansPerSecond speed{ 100.0f };
    foc::Radians angle{ 0.5f };
    foc::NewtonMeter torque{ 0.2f };

    tuner.Update(currents, speed, angle, torque);

    auto& foc = tuner.GetFoc();
    EXPECT_EQ(foc.tuningsSetCount, 0);
}

TEST_F(TestRealTimeSpeedPidTune, pid_gains_calculated_correctly_when_converged)
{
    config.innovationThreshold = 1.0f;
    config.uncertityThreshold = 1.0f;
    config.speedSamplingFrequency = hal::Hertz{ 1000 };

    TestRealTimeSpeedPidTuneImpl tuner(config);

    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::RadiansPerSecond speed{ 100.0f };
    foc::Radians angle{ 0.5f };
    foc::NewtonMeter torque{ 0.2f };

    tuner.Update(currents, speed, angle, torque);

    auto& foc = tuner.GetFoc();
    EXPECT_EQ(foc.tuningsSetCount, 1);
    EXPECT_EQ(foc.lastVdc.Value(), 24.0f);

    float expectedWc = 1000.0f * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = expectedWc * 0.01f;
    float expectedKi = expectedWc * 0.001f;

    EXPECT_NEAR(foc.lastTunings.kp, expectedKp, 0.001f);
    EXPECT_NEAR(foc.lastTunings.ki, expectedKi, 0.0001f);
    EXPECT_EQ(foc.lastTunings.kd, 0.0f);
}
