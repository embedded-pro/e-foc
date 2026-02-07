#include "numerical/math/Tolerance.hpp"
#include "source/foc/implementations/RealTimeFrictionAndInertiaEstimator.hpp"
#include <gmock/gmock.h>

namespace
{
    class TestRealTimeFrictionAndInertiaEstimator
        : public ::testing::Test
    {
    public:
        foc::RealTimeFrictionAndInertiaEstimator estimator{ 0.99f, hal::Hertz{ 1000 } };
    };
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, update_produces_finite_results)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::RadiansPerSecond speed{ 10.0f };
    foc::Radians angle{ 0.0f };
    foc::NewtonMeter torque{ 0.1f };

    auto result = estimator.Update(currents, speed, angle, torque);

    EXPECT_TRUE(std::isfinite(result.inertia.Value()));
    EXPECT_TRUE(std::isfinite(result.friction.Value()));
    EXPECT_TRUE(std::isfinite(result.metrics.innovation));
    EXPECT_TRUE(std::isfinite(result.metrics.residual));
    EXPECT_TRUE(std::isfinite(result.metrics.uncertainty));
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, acceleration_calculated_from_speed_difference)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::Radians angle{ 0.0f };
    foc::NewtonMeter torque{ 0.1f };

    estimator.Update(currents, foc::RadiansPerSecond{ 10.0f }, angle, torque);
    auto result = estimator.Update(currents, foc::RadiansPerSecond{ 20.0f }, angle, torque);

    EXPECT_TRUE(std::isfinite(result.inertia.Value()));
    EXPECT_TRUE(std::isfinite(result.friction.Value()));
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, result_units_are_correct_types)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::RadiansPerSecond speed{ 50.0f };
    foc::Radians angle{ 0.5f };
    foc::NewtonMeter torque{ 0.15f };

    auto result = estimator.Update(currents, speed, angle, torque);

    foc::NewtonMeterSecondPerRadian inertia = result.inertia;
    foc::NewtonMeterSecondSquared friction = result.friction;

    EXPECT_TRUE(std::isfinite(inertia.Value()));
    EXPECT_TRUE(std::isfinite(friction.Value()));
}
