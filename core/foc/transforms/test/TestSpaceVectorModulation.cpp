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

    constexpr float sectorWidth = std::numbers::pi_v<float> / 3.0f;

    foc::TwoPhase VectorAt(float magnitude, float angle)
    {
        return { magnitude * std::cos(angle), magnitude * std::sin(angle) };
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

// A 60 degree boundary hands the reference over to the next pair of active vectors. The commanded
// voltage does not jump there, so no phase duty may either: each is sampled either side of it.
TEST_F(TestSpaceVectorModulation, duty_cycles_are_continuous_across_every_sector_boundary)
{
    constexpr float magnitude = 0.9f;
    constexpr float deltaAngle = 1e-3f;
    constexpr float maxStep = 5e-3f;

    for (int boundary = 0; boundary != 6; ++boundary)
    {
        SCOPED_TRACE(boundary);
        const float angle = static_cast<float>(boundary) * sectorWidth;

        auto below = spaceVectorModulation->Generate(VectorAt(magnitude, angle - deltaAngle));
        auto above = spaceVectorModulation->Generate(VectorAt(magnitude, angle + deltaAngle));

        EXPECT_NEAR(below.a, above.a, maxStep);
        EXPECT_NEAR(below.b, above.b, maxStep);
        EXPECT_NEAR(below.c, above.c, maxStep);
    }
}

// The alpha axis aligns with active vector V1, so 0, 60, ... 300 degrees are the hexagon vertex
// directions. Driven to the vertex reach |V| = 2/sqrt(3) the modulator must collapse onto exactly
// the switching state of that vertex: V1 = 100, V2 = 110, V3 = 010, V4 = 011, V5 = 001, V6 = 101.
TEST_F(TestSpaceVectorModulation, active_vector_directions_collapse_onto_a_pure_switching_state)
{
    constexpr float tolerance = 1e-5f;
    const float vertexReach = 2.0f / std::numbers::sqrt3_v<float>;
    constexpr std::array<std::array<float, 3>, 6> switchingStates{ {
        { 1.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f },
        { 1.0f, 0.0f, 1.0f },
    } };

    for (int vertex = 0; vertex != 6; ++vertex)
    {
        SCOPED_TRACE(vertex);
        auto pwm = spaceVectorModulation->Generate(VectorAt(vertexReach, static_cast<float>(vertex) * sectorWidth));

        EXPECT_NEAR(pwm.a, switchingStates[vertex][0], tolerance);
        EXPECT_NEAR(pwm.b, switchingStates[vertex][1], tolerance);
        EXPECT_NEAR(pwm.c, switchingStates[vertex][2], tolerance);
    }
}

// Halfway between two active vectors the inscribed circle touches the hexagon edge, so |V| = 1 is
// exactly the linear-modulation limit: the two phases spanning the sector sit at 1 and 0 while the
// phase orthogonal to the reference stays centred at 0.5.
TEST_F(TestSpaceVectorModulation, sector_centres_reach_the_limit_at_unit_magnitude)
{
    constexpr float tolerance = 1e-5f;
    constexpr std::array<std::array<float, 3>, 6> centreDuties{ {
        { 1.0f, 0.5f, 0.0f },
        { 0.5f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.5f },
        { 0.0f, 0.5f, 1.0f },
        { 0.5f, 0.0f, 1.0f },
        { 1.0f, 0.0f, 0.5f },
    } };

    for (int sector = 0; sector != 6; ++sector)
    {
        SCOPED_TRACE(sector);
        auto pwm = spaceVectorModulation->Generate(VectorAt(1.0f, (static_cast<float>(sector) + 0.5f) * sectorWidth));

        EXPECT_NEAR(pwm.a, centreDuties[sector][0], tolerance);
        EXPECT_NEAR(pwm.b, centreDuties[sector][1], tolerance);
        EXPECT_NEAR(pwm.c, centreDuties[sector][2], tolerance);
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
