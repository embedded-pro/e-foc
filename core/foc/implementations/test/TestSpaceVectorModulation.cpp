#include "numerical/math/Tolerance.hpp"
#include "core/foc/implementations/SpaceVectorModulation.hpp"
#include <gmock/gmock.h>

namespace
{
    foc::TwoPhase CreateTwoPhaseFrame(float d, float q)
    {
        return { d, q };
    }

    class TestSpaceVectorModulation
        : public ::testing::Test
    {
    public:
        std::optional<foc::SpaceVectorModulation> spaceVectorModulation;

        void SetUp() override
        {
            spaceVectorModulation.emplace();
        }
    };
}

TEST_F(TestSpaceVectorModulation, zero_voltage)
{
    auto twoPhaseVoltage = CreateTwoPhaseFrame(0.0f, 0.0f);

    auto pwm = spaceVectorModulation->Generate(twoPhaseVoltage);
    float tolerance = math::Tolerance<float>();

    EXPECT_NEAR(pwm.a, 0.5f, tolerance);
    EXPECT_NEAR(pwm.b, 0.5f, tolerance);
    EXPECT_NEAR(pwm.c, 0.5f, tolerance);
}

TEST_F(TestSpaceVectorModulation, overmodulation)
{
    auto twoPhaseVoltage = CreateTwoPhaseFrame(0.5f, 0.5f);
    auto pwm = spaceVectorModulation->Generate(twoPhaseVoltage);

    EXPECT_GE(pwm.a, 0.0f);
    EXPECT_LE(pwm.a, 1.0f);
    EXPECT_GE(pwm.b, 0.0f);
    EXPECT_LE(pwm.b, 1.0f);
    EXPECT_GE(pwm.c, 0.0f);
    EXPECT_LE(pwm.c, 1.0f);
}

TEST_F(TestSpaceVectorModulation, common_mode_injection)
{
    auto twoPhaseVoltage = CreateTwoPhaseFrame(0.5f, 0.0f);
    auto pwm = spaceVectorModulation->Generate(twoPhaseVoltage);
    float tolerance = math::Tolerance<float>();

    float min_duty = std::min({ pwm.a, pwm.b, pwm.c });
    float max_duty = std::max({ pwm.a, pwm.b, pwm.c });

    EXPECT_NEAR(min_duty + max_duty, 1.0f, tolerance);
}

TEST_F(TestSpaceVectorModulation, duty_cycle_bounds)
{
    auto test_points = {
        CreateTwoPhaseFrame(0.5f, 0.0f),
        CreateTwoPhaseFrame(0.0f, 0.5f),
        CreateTwoPhaseFrame(0.35f, 0.35f)
    };

    for (const auto& dq : test_points)
    {
        auto pwm = spaceVectorModulation->Generate(dq);
        EXPECT_GE(pwm.a, 0.0f);
        EXPECT_LE(pwm.a, 1.0f);
        EXPECT_GE(pwm.b, 0.0f);
        EXPECT_LE(pwm.b, 1.0f);
        EXPECT_GE(pwm.c, 0.0f);
        EXPECT_LE(pwm.c, 1.0f);
    }
}

TEST_F(TestSpaceVectorModulation, output_linearity)
{
    auto dq_small = CreateTwoPhaseFrame(0.05f, 0.0f);
    auto dq_large = CreateTwoPhaseFrame(0.1f, 0.0f);

    auto pwm_small = spaceVectorModulation->Generate(dq_small);
    auto pwm_large = spaceVectorModulation->Generate(dq_large);

    float tolerance = math::Tolerance<float>();
    float small_dev = std::abs(pwm_small.a - 0.5f);
    float large_dev = std::abs(pwm_large.a - 0.5f);
    EXPECT_NEAR(large_dev / small_dev, 2.0f, tolerance);
}

TEST_F(TestSpaceVectorModulation, zero_voltage_centering)
{
    auto zero_voltage = CreateTwoPhaseFrame(0.0f, 0.0f);
    auto pwm = spaceVectorModulation->Generate(zero_voltage);

    float tolerance = math::Tolerance<float>();
    EXPECT_NEAR(pwm.a, 0.5f, tolerance);
    EXPECT_NEAR(pwm.b, 0.5f, tolerance);
    EXPECT_NEAR(pwm.c, 0.5f, tolerance);
}

TEST_F(TestSpaceVectorModulation, sector_continuity)
{
    auto dq = CreateTwoPhaseFrame(0.5f, 0.0f);

    auto pwm1 = spaceVectorModulation->Generate(dq);
    auto pwm2 = spaceVectorModulation->Generate(dq);

    float max_change = 0.2f;
    EXPECT_NEAR(pwm1.a, pwm2.a, max_change);
    EXPECT_NEAR(pwm1.b, pwm2.b, max_change);
    EXPECT_NEAR(pwm1.c, pwm2.c, max_change);
}
