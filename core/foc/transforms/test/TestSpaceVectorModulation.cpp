#include "core/foc/transforms/SpaceVectorModulation.hpp"
#include "numerical/math/Tolerance.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

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

// The alpha axis aligns with active vector V1, so 0, 60, ... 300 degrees are the hexagon vertex
// directions. Driven to the vertex reach |V| = 2/sqrt(3) the modulator must collapse onto a pure
// inverter switching state: every duty is exactly 0 or 1, with at least one of each.
TEST_F(TestSpaceVectorModulation, active_vector_directions_collapse_onto_a_pure_switching_state)
{
    const float tolerance = 1e-5f;
    const float vertexReach = 2.0f / std::numbers::sqrt3_v<float>;

    for (int vertex = 0; vertex < 6; ++vertex)
    {
        const float angle = static_cast<float>(vertex) * std::numbers::pi_v<float> / 3.0f;
        auto pwm = spaceVectorModulation->Generate(
            CreateTwoPhaseFrame(vertexReach * std::cos(angle), vertexReach * std::sin(angle)));

        std::array<float, 3> duties{ pwm.a, pwm.b, pwm.c };
        std::sort(duties.begin(), duties.end());

        EXPECT_NEAR(duties[0], 0.0f, tolerance) << "vertex " << vertex;
        EXPECT_NEAR(duties[2], 1.0f, tolerance) << "vertex " << vertex;
        EXPECT_TRUE(std::abs(duties[1]) < tolerance || std::abs(duties[1] - 1.0f) < tolerance)
            << "vertex " << vertex << " middle duty " << duties[1];
    }
}

// Halfway between two active vectors the inscribed circle touches the hexagon edge, so |V| = 1 is
// exactly the linear-modulation limit: one phase at 1, one at 0, and the third centred at 0.5.
TEST_F(TestSpaceVectorModulation, sector_centres_reach_the_limit_at_unit_magnitude)
{
    const float tolerance = 1e-5f;

    for (int sector = 0; sector < 6; ++sector)
    {
        const float angle = (static_cast<float>(sector) + 0.5f) * std::numbers::pi_v<float> / 3.0f;
        auto pwm = spaceVectorModulation->Generate(CreateTwoPhaseFrame(std::cos(angle), std::sin(angle)));

        std::array<float, 3> duties{ pwm.a, pwm.b, pwm.c };
        std::sort(duties.begin(), duties.end());

        EXPECT_NEAR(duties[0], 0.0f, tolerance) << "sector " << sector;
        EXPECT_NEAR(duties[1], 0.5f, tolerance) << "sector " << sector;
        EXPECT_NEAR(duties[2], 1.0f, tolerance) << "sector " << sector;
    }
}

TEST_F(TestSpaceVectorModulation, duty_cycles_stay_within_range_across_a_full_revolution)
{
    for (int step = 0; step < 360; ++step)
    {
        const float angle = static_cast<float>(step) * std::numbers::pi_v<float> / 180.0f;
        auto pwm = spaceVectorModulation->Generate(CreateTwoPhaseFrame(std::cos(angle), std::sin(angle)));

        EXPECT_GE(std::min({ pwm.a, pwm.b, pwm.c }), 0.0f) << "step " << step;
        EXPECT_LE(std::max({ pwm.a, pwm.b, pwm.c }), 1.0f) << "step " << step;
    }
}
