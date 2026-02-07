#include "numerical/estimators/Estimator.hpp"
#include "source/foc/implementations/WithRealTimeSpeedPidTune.hpp"
#include "source/foc/interfaces/FrictionAndInertiaEstimator.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    class MockFocWithSpeedControl
        : public foc::FieldOrientedControllerSpeedControl
    {
    public:
        MOCK_METHOD(void, SetPolePairs, (std::size_t), (override));
        MOCK_METHOD(void, Reset, (), (override));
        MOCK_METHOD(void, SetCurrentTunings, (foc::Volts, const foc::IdAndIqTunings&), (override));
        MOCK_METHOD(foc::PhasePwmDutyCycles, Calculate, (const foc::PhaseCurrents&, foc::Radians&), (override));
        MOCK_METHOD(void, SetPoint, (foc::RadiansPerSecond), (override));
        MOCK_METHOD(void, SetSpeedTunings, (foc::Volts, const foc::SpeedTunings&), (override));
    };

    class MockFrictionAndInertiaEstimator
        : public foc::FrictionAndInertiaEstimator
    {
    public:
        MOCK_METHOD(foc::FrictionAndInertiaEstimator::Result, Update, (foc::PhaseCurrents, foc::RadiansPerSecond, foc::Radians, foc::NewtonMeter), (override));
    };

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

        MockFocWithSpeedControl mockFoc;
        MockFrictionAndInertiaEstimator mockEstimator;
        foc::WithRealTimeSpeedPidTune::Config config;
    };
}

TEST_F(TestRealTimeSpeedPidTune, estimator_update_is_called_with_correct_parameters)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::RadiansPerSecond speed{ 100.0f };
    foc::Radians angle{ 0.5f };
    foc::NewtonMeter torque{ 0.2f };

    foc::FrictionAndInertiaEstimator::Result result;
    result.inertia = foc::NewtonMeterSecondPerRadian{ 0.01f };
    result.friction = foc::NewtonMeterSecondSquared{ 0.001f };
    result.metrics.innovation = 0.1f;
    result.metrics.residual = 0.05f;
    result.metrics.uncertainty = 0.2f;

    EXPECT_CALL(mockEstimator, Update(
        testing::AllOf(
            testing::Field(&foc::PhaseCurrents::a, foc::Ampere{ 1.0f }),
            testing::Field(&foc::PhaseCurrents::b, foc::Ampere{ -0.5f }),
            testing::Field(&foc::PhaseCurrents::c, foc::Ampere{ -0.5f })),
        foc::RadiansPerSecond{ 100.0f },
        foc::Radians{ 0.5f },
        foc::NewtonMeter{ 0.2f }))
        .WillOnce(testing::Return(result));

    foc::WithRealTimeSpeedPidTune tuner(mockFoc, mockEstimator, config);
    tuner.Update(currents, speed, angle, torque);
}

TEST_F(TestRealTimeSpeedPidTune, tunings_not_set_when_not_converged)
{
    config.innovationThreshold = 0.001f;
    config.uncertityThreshold = 0.05f;

    foc::FrictionAndInertiaEstimator::Result result;
    result.inertia = foc::NewtonMeterSecondPerRadian{ 0.01f };
    result.friction = foc::NewtonMeterSecondSquared{ 0.001f };
    result.metrics.innovation = 0.1f;
    result.metrics.residual = 0.05f;
    result.metrics.uncertainty = 0.2f;

    EXPECT_CALL(mockEstimator, Update(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(result));
    EXPECT_CALL(mockFoc, SetSpeedTunings(testing::_, testing::_)).Times(0);

    foc::WithRealTimeSpeedPidTune tuner(mockFoc, mockEstimator, config);

    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::RadiansPerSecond speed{ 100.0f };
    foc::Radians angle{ 0.5f };
    foc::NewtonMeter torque{ 0.2f };

    tuner.Update(currents, speed, angle, torque);
}

TEST_F(TestRealTimeSpeedPidTune, pid_gains_calculated_correctly_when_converged)
{
    config.innovationThreshold = 1.0f;
    config.uncertityThreshold = 1.0f;
    config.speedSamplingFrequency = hal::Hertz{ 1000 };

    float expectedWc = 1000.0f * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = expectedWc * 0.01f;
    float expectedKi = expectedWc * 0.001f;

    foc::FrictionAndInertiaEstimator::Result result;
    result.inertia = foc::NewtonMeterSecondPerRadian{ 0.01f };
    result.friction = foc::NewtonMeterSecondSquared{ 0.001f };
    result.metrics.innovation = 0.001f;
    result.metrics.residual = 0.0f;
    result.metrics.uncertainty = 0.05f;

    EXPECT_CALL(mockEstimator, Update(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(result));
    EXPECT_CALL(mockFoc, SetSpeedTunings(foc::Volts{ 24.0f }, testing::AllOf(
                                                                  testing::Field(&foc::SpeedTunings::kp, testing::FloatNear(expectedKp, 0.001f)),
                                                                  testing::Field(&foc::SpeedTunings::ki, testing::FloatNear(expectedKi, 0.0001f)),
                                                                  testing::Field(&foc::SpeedTunings::kd, 0.0f))))
        .Times(1);

    foc::WithRealTimeSpeedPidTune tuner(mockFoc, mockEstimator, config);

    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::RadiansPerSecond speed{ 100.0f };
    foc::Radians angle{ 0.5f };
    foc::NewtonMeter torque{ 0.2f };

    tuner.Update(currents, speed, angle, torque);
}
