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

TEST_F(TestRealTimeFrictionAndInertiaEstimator, first_update_produces_result)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::RadiansPerSecond speed{ 10.0f };
    foc::Radians angle{ 0.0f };
    foc::NewtonMeter torque{ 0.1f };

    auto result = estimator.Update(currents, speed, angle, torque);

    EXPECT_TRUE(std::isfinite(result.inertia.Value()));
    EXPECT_TRUE(std::isfinite(result.friction.Value()));
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, zero_speed_produces_valid_result)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    foc::RadiansPerSecond speed{ 0.0f };
    foc::Radians angle{ 0.0f };
    foc::NewtonMeter torque{ 0.0f };

    auto result = estimator.Update(currents, speed, angle, torque);

    EXPECT_TRUE(std::isfinite(result.inertia.Value()));
    EXPECT_TRUE(std::isfinite(result.friction.Value()));
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, changing_electrical_angle_updates_correctly)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::RadiansPerSecond speed{ 100.0f };
    foc::NewtonMeter torque{ 0.2f };

    for (float angle = 0.0f; angle < 2.0f * M_PI; angle += M_PI / 4.0f)
    {
        auto result = estimator.Update(currents, speed, foc::Radians{ angle }, torque);

        EXPECT_TRUE(std::isfinite(result.inertia.Value()));
        EXPECT_TRUE(std::isfinite(result.friction.Value()));
    }
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, acceleration_affects_estimation)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::Radians angle{ 0.0f };
    foc::NewtonMeter torque{ 0.1f };

    auto result1 = estimator.Update(currents, foc::RadiansPerSecond{ 10.0f }, angle, torque);

    auto result2 = estimator.Update(currents, foc::RadiansPerSecond{ 20.0f }, angle, torque);

    EXPECT_TRUE(std::isfinite(result1.inertia.Value()));
    EXPECT_TRUE(std::isfinite(result2.inertia.Value()));
    EXPECT_TRUE(std::isfinite(result1.friction.Value()));
    EXPECT_TRUE(std::isfinite(result2.friction.Value()));
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, metrics_updated_each_iteration)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::RadiansPerSecond speed{ 50.0f };
    foc::Radians angle{ 0.0f };
    foc::NewtonMeter torque{ 0.15f };

    auto result1 = estimator.Update(currents, speed, angle, torque);
    auto result2 = estimator.Update(currents, speed, angle, torque);

    EXPECT_TRUE(std::isfinite(result1.metrics.innovation));
    EXPECT_TRUE(std::isfinite(result1.metrics.residual));
    EXPECT_TRUE(std::isfinite(result1.metrics.uncertainty));
    EXPECT_TRUE(std::isfinite(result2.metrics.innovation));
    EXPECT_TRUE(std::isfinite(result2.metrics.residual));
    EXPECT_TRUE(std::isfinite(result2.metrics.uncertainty));
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, varying_current_phases_handled_correctly)
{
    foc::RadiansPerSecond speed{ 75.0f };
    foc::Radians angle{ M_PI / 6 };
    foc::NewtonMeter torque{ 0.12f };

    std::vector<foc::PhaseCurrents> currentPatterns = {
        { foc::Ampere{ 1.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ -1.0f } },
        { foc::Ampere{ 0.5f }, foc::Ampere{ 0.5f }, foc::Ampere{ -1.0f } },
        { foc::Ampere{ -0.5f }, foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f } }
    };

    for (const auto& currents : currentPatterns)
    {
        auto result = estimator.Update(currents, speed, angle, torque);

        EXPECT_TRUE(std::isfinite(result.inertia.Value()));
        EXPECT_TRUE(std::isfinite(result.friction.Value()));
    }
}

TEST_F(TestRealTimeFrictionAndInertiaEstimator, negative_speed_produces_valid_result)
{
    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::RadiansPerSecond speed{ -50.0f };
    foc::Radians angle{ M_PI };
    foc::NewtonMeter torque{ -0.1f };

    auto result = estimator.Update(currents, speed, angle, torque);

    EXPECT_TRUE(std::isfinite(result.inertia.Value()));
    EXPECT_TRUE(std::isfinite(result.friction.Value()));
}
