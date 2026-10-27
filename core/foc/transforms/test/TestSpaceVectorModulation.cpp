#include "core/foc/transforms/SpaceVectorModulation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <gmock/gmock.h>
#include <limits>
#include <numbers>

namespace
{
    constexpr float oneStep = 1.0f / static_cast<float>(hal::DutyCycle::fullScale);

    foc::TwoPhase CreateTwoPhaseFrame(float d, float q)
    {
        return { d, q };
    }

    constexpr float sectorWidth = std::numbers::pi_v<float> / 3.0f;

    foc::TwoPhase VectorAt(float magnitude, float angle)
    {
        return { magnitude * std::cos(angle), magnitude * std::sin(angle) };
    }

    float Fraction(hal::DutyCycle duty)
    {
        return foc::DutyFraction(duty);
    }

    float DutyPercent(hal::DutyCycle duty)
    {
        return 100.0f * foc::DutyFraction(duty);
    }

    class TestSpaceVectorModulation
        : public ::testing::Test
    {
    public:
        foc::SpaceVectorModulation spaceVectorModulation;
    };

    class TestDutyConversion
        : public ::testing::Test
    {
    public:
        static float Nan()
        {
            return std::numeric_limits<float>::quiet_NaN();
        }

        static float Inf()
        {
            return std::numeric_limits<float>::infinity();
        }
    };
}

TEST_F(TestSpaceVectorModulation, zero_voltage)
{
    auto pwm = spaceVectorModulation.Generate(CreateTwoPhaseFrame(0.0f, 0.0f));

    EXPECT_NEAR(Fraction(pwm.a), 0.5f, oneStep);
    EXPECT_NEAR(Fraction(pwm.b), 0.5f, oneStep);
    EXPECT_NEAR(Fraction(pwm.c), 0.5f, oneStep);
}

TEST_F(TestSpaceVectorModulation, overmodulation)
{
    auto pwm = spaceVectorModulation.Generate(CreateTwoPhaseFrame(0.5f, 0.5f));

    EXPECT_LE(pwm.a.Value(), hal::DutyCycle::fullScale);
    EXPECT_LE(pwm.b.Value(), hal::DutyCycle::fullScale);
    EXPECT_LE(pwm.c.Value(), hal::DutyCycle::fullScale);
}

TEST_F(TestSpaceVectorModulation, common_mode_injection)
{
    auto pwm = spaceVectorModulation.Generate(CreateTwoPhaseFrame(0.5f, 0.0f));

    float minDuty = std::min({ Fraction(pwm.a), Fraction(pwm.b), Fraction(pwm.c) });
    float maxDuty = std::max({ Fraction(pwm.a), Fraction(pwm.b), Fraction(pwm.c) });

    EXPECT_NEAR(minDuty + maxDuty, 1.0f, 2.0f * oneStep);
}

TEST_F(TestSpaceVectorModulation, duty_cycle_bounds)
{
    auto testPoints = {
        CreateTwoPhaseFrame(0.5f, 0.0f),
        CreateTwoPhaseFrame(0.0f, 0.5f),
        CreateTwoPhaseFrame(0.35f, 0.35f)
    };

    for (const auto& dq : testPoints)
    {
        auto pwm = spaceVectorModulation.Generate(dq);
        EXPECT_LE(pwm.a.Value(), hal::DutyCycle::fullScale);
        EXPECT_LE(pwm.b.Value(), hal::DutyCycle::fullScale);
        EXPECT_LE(pwm.c.Value(), hal::DutyCycle::fullScale);
    }
}

TEST_F(TestSpaceVectorModulation, output_linearity)
{
    auto pwmSmall = spaceVectorModulation.Generate(CreateTwoPhaseFrame(0.05f, 0.0f));
    auto pwmLarge = spaceVectorModulation.Generate(CreateTwoPhaseFrame(0.1f, 0.0f));

    float smallDeviation = std::abs(Fraction(pwmSmall.a) - 0.5f);
    float largeDeviation = std::abs(Fraction(pwmLarge.a) - 0.5f);
    EXPECT_NEAR(largeDeviation / smallDeviation, 2.0f, 1e-3f);
}

TEST_F(TestSpaceVectorModulation, duty_cycles_are_continuous_across_every_sector_boundary)
{
    constexpr float magnitude = 0.9f;
    constexpr float deltaAngle = 1e-3f;
    constexpr float maxStep = 5e-3f;

    for (int boundary = 0; boundary != 6; ++boundary)
    {
        SCOPED_TRACE(boundary);
        const float angle = static_cast<float>(boundary) * sectorWidth;

        auto below = spaceVectorModulation.Generate(VectorAt(magnitude, angle - deltaAngle));
        auto above = spaceVectorModulation.Generate(VectorAt(magnitude, angle + deltaAngle));

        EXPECT_NEAR(Fraction(below.a), Fraction(above.a), maxStep);
        EXPECT_NEAR(Fraction(below.b), Fraction(above.b), maxStep);
        EXPECT_NEAR(Fraction(below.c), Fraction(above.c), maxStep);
    }
}

TEST_F(TestSpaceVectorModulation, active_vector_directions_collapse_onto_a_pure_switching_state)
{
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
        auto pwm = spaceVectorModulation.Generate(VectorAt(vertexReach, static_cast<float>(vertex) * sectorWidth));

        EXPECT_NEAR(Fraction(pwm.a), switchingStates[vertex][0], oneStep);
        EXPECT_NEAR(Fraction(pwm.b), switchingStates[vertex][1], oneStep);
        EXPECT_NEAR(Fraction(pwm.c), switchingStates[vertex][2], oneStep);
    }
}

TEST_F(TestSpaceVectorModulation, sector_centres_reach_the_limit_at_unit_magnitude)
{
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
        auto pwm = spaceVectorModulation.Generate(VectorAt(1.0f, (static_cast<float>(sector) + 0.5f) * sectorWidth));

        EXPECT_NEAR(Fraction(pwm.a), centreDuties[sector][0], oneStep);
        EXPECT_NEAR(Fraction(pwm.b), centreDuties[sector][1], oneStep);
        EXPECT_NEAR(Fraction(pwm.c), centreDuties[sector][2], oneStep);
    }
}

TEST_F(TestSpaceVectorModulation, duty_cycles_stay_within_range_across_a_full_revolution)
{
    for (int step = 0; step < 360; ++step)
    {
        const float angle = static_cast<float>(step) * std::numbers::pi_v<float> / 180.0f;
        auto pwm = spaceVectorModulation.Generate(CreateTwoPhaseFrame(std::cos(angle), std::sin(angle)));

        EXPECT_LE(std::max({ pwm.a.Value(), pwm.b.Value(), pwm.c.Value() }), hal::DutyCycle::fullScale) << "step " << step;
    }
}

TEST_F(TestDutyConversion, a_normalised_output_maps_onto_the_percent_range)
{
    const auto duties = foc::ToDutyCycles(foc::ThreePhase{ 0.0f, 0.5f, 1.0f });

    EXPECT_EQ(DutyPercent(duties.a), 0);
    EXPECT_EQ(DutyPercent(duties.b), 50);
    EXPECT_EQ(DutyPercent(duties.c), 100);
}

TEST_F(TestDutyConversion, a_normalised_output_rounds_to_the_nearest_q16_step)
{
    const auto duties = foc::ToDutyCycles(foc::ThreePhase{ 0.5004f, 0.12345f, 0.9999f });

    EXPECT_EQ(duties.a.Value(), 32794u);
    EXPECT_EQ(duties.b.Value(), 8090u);
    EXPECT_EQ(duties.c.Value(), 65529u);
}

TEST_F(TestDutyConversion, a_normalised_output_keeps_sub_percent_resolution)
{
    const auto duties = foc::ToDutyCycles(foc::ThreePhase{ 0.5004f, 0.12345f, 0.9999f });

    constexpr float oneStepInPercent = 100.0f * oneStep;
    EXPECT_NEAR(DutyPercent(duties.a), 50.04f, oneStepInPercent);
    EXPECT_NEAR(DutyPercent(duties.b), 12.345f, oneStepInPercent);
    EXPECT_NEAR(DutyPercent(duties.c), 99.99f, oneStepInPercent);
}

TEST_F(TestDutyConversion, an_output_beyond_the_range_is_clamped_rather_than_wrapped)
{
    const auto duties = foc::ToDutyCycles(foc::ThreePhase{ -5.0f, 1.5f, 0.25f });

    EXPECT_EQ(DutyPercent(duties.a), 0);
    EXPECT_EQ(DutyPercent(duties.b), 100);
    EXPECT_EQ(DutyPercent(duties.c), 25);
}

TEST_F(TestDutyConversion, a_nan_modulation_lands_on_zero_duty)
{
    const auto duties = foc::ToDutyCycles(foc::ThreePhase{ Nan(), Nan(), Nan() });

    EXPECT_EQ(DutyPercent(duties.a), 0);
    EXPECT_EQ(DutyPercent(duties.b), 0);
    EXPECT_EQ(DutyPercent(duties.c), 0);
}

TEST_F(TestDutyConversion, an_infinite_modulation_saturates_to_a_defined_endpoint)
{
    const auto duties = foc::ToDutyCycles(foc::ThreePhase{ Inf(), -Inf(), 0.5f });

    EXPECT_EQ(DutyPercent(duties.a), 100);
    EXPECT_EQ(DutyPercent(duties.b), 0);
    EXPECT_EQ(DutyPercent(duties.c), 50);
}
