#include "core/foc/cascade/PositionProfile.hpp"
#include <algorithm>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    const hal::Hertz outerFrequency{ 1000 };
    constexpr float period{ 0.001f };
    constexpr float pi{ std::numbers::pi_v<float> };

    class TestPositionProfile
        : public ::testing::Test
    {
    public:
        void Profile(float velocity, float acceleration, float deceleration = 0.0f, float jerk = 0.0f)
        {
            profile.Limit(foc::RadiansPerSecond{ velocity }, foc::RadiansPerSecondSquared{ acceleration }, foc::RadiansPerSecondSquared{ deceleration }, foc::RadiansPerSecondCubed{ jerk });
        }

        foc::ProfiledReference Advance(float measured = 0.0f, float measuredSpeed = 0.0f)
        {
            return profile.Advance(travel, foc::Radians{ measured }, foc::RadiansPerSecond{ measuredSpeed });
        }

        foc::ProfiledReference Run(std::size_t ticks, float measured = 0.0f)
        {
            foc::ProfiledReference last;
            for (std::size_t tick = 0; tick != ticks; ++tick)
                last = Advance(measured);
            return last;
        }

        foc::PositionProfile profile{ outerFrequency };
        foc::TravelLimits travel;
    };
}

TEST_F(TestPositionProfile, a_profile_without_limits_passes_the_target_through_at_rest)
{
    profile.SetTarget(foc::Radians{ 2.5f });

    const auto reference = Advance();

    EXPECT_FALSE(profile.IsActive());
    EXPECT_EQ(reference.position.Value(), 2.5f);
    EXPECT_EQ(reference.velocity.Value(), 0.0f);
    EXPECT_EQ(reference.acceleration.Value(), 0.0f);
}

TEST_F(TestPositionProfile, a_velocity_without_an_acceleration_leaves_the_profile_off)
{
    Profile(20.0f, 0.0f);

    EXPECT_FALSE(profile.IsActive());
}

TEST_F(TestPositionProfile, a_zero_deceleration_uses_the_acceleration)
{
    Profile(20.0f, 400.0f, 0.0f);
    profile.SetTarget(foc::Radians{ 1.5f });

    Run(10);
    const auto reference = Run(200);

    EXPECT_NEAR(reference.position.Value(), 1.5f, 1e-4f);
}

TEST_F(TestPositionProfile, a_move_of_one_and_a_half_radians_reaches_its_target_in_125_ms)
{
    Profile(20.0f, 400.0f);
    profile.SetTarget(foc::Radians{ 1.5f });

    std::size_t ticks{ 0 };
    float peakSpeed{ 0.0f };
    float peakAcceleration{ 0.0f };
    foc::ProfiledReference reference;
    do
    {
        reference = Advance();
        peakSpeed = std::max(peakSpeed, reference.velocity.Value());
        peakAcceleration = std::max(peakAcceleration, std::abs(reference.acceleration.Value()));
        ++ticks;
    } while (reference.position.Value() < 1.5f - 1e-4f && ticks < 1000);

    EXPECT_NEAR(static_cast<float>(ticks) * period, 0.125f, 0.001f);
    EXPECT_NEAR(peakSpeed, 20.0f, 1e-3f);
    EXPECT_NEAR(peakAcceleration, 400.0f, 1e-3f);
}

TEST_F(TestPositionProfile, the_target_is_held_at_rest_once_reached)
{
    Profile(20.0f, 400.0f);
    profile.SetTarget(foc::Radians{ 1.5f });

    const auto reference = Run(500);

    EXPECT_NEAR(reference.position.Value(), 1.5f, 1e-5f);
    EXPECT_EQ(reference.velocity.Value(), 0.0f);
    EXPECT_EQ(reference.acceleration.Value(), 0.0f);
}

TEST_F(TestPositionProfile, the_profile_starts_from_the_measured_position)
{
    Profile(20.0f, 400.0f);
    profile.SetTarget(foc::Radians{ 1.0f });

    const auto reference = Advance(0.8f);

    EXPECT_GT(reference.position.Value(), 0.8f);
    EXPECT_LT(reference.position.Value(), 0.801f);
}

TEST_F(TestPositionProfile, a_target_across_the_wrap_takes_the_shorter_way)
{
    Profile(20.0f, 400.0f);
    profile.SetTarget(foc::Radians{ -pi + 0.2f });

    const auto first = Advance(pi - 0.2f);
    const auto last = Run(500, pi - 0.2f);

    EXPECT_GT(first.velocity.Value(), 0.0f);
    EXPECT_NEAR(std::remainder(last.position.Value() - (-pi + 0.2f), 2.0f * pi), 0.0f, 1e-4f);
}

TEST_F(TestPositionProfile, the_reference_stays_inside_one_turn)
{
    Profile(20.0f, 400.0f);
    profile.SetTarget(foc::Radians{ -pi + 0.2f });
    Advance(pi - 0.2f);

    float peak{ 0.0f };
    for (std::size_t tick = 0; tick != 500; ++tick)
        peak = std::max(peak, std::abs(Advance(pi - 0.2f).position.Value()));

    EXPECT_LE(peak, pi + 1e-5f);
}

TEST_F(TestPositionProfile, travel_limits_make_the_move_go_the_direct_way)
{
    travel = foc::TravelLimits{ foc::Radians{ -3.0f }, foc::Radians{ 3.0f } };
    Profile(20.0f, 400.0f);
    profile.SetTarget(foc::Radians{ -2.9f });

    const auto first = Advance(2.9f);
    float lowest{ first.position.Value() };
    foc::ProfiledReference last;
    for (std::size_t tick = 0; tick != 1000; ++tick)
    {
        last = Advance(2.9f);
        lowest = std::min(lowest, last.position.Value());
    }

    EXPECT_LT(first.velocity.Value(), 0.0f);
    EXPECT_NEAR(last.position.Value(), -2.9f, 1e-4f);
    EXPECT_GE(lowest, -2.9f - 1e-4f);
}

TEST_F(TestPositionProfile, travel_limits_clamp_a_target_outside_them)
{
    travel = foc::TravelLimits{ foc::Radians{ -1.0f }, foc::Radians{ 1.0f } };
    Profile(20.0f, 400.0f);
    profile.SetTarget(foc::Radians{ 2.0f });

    const auto reference = Run(500);

    EXPECT_NEAR(reference.position.Value(), 1.0f, 1e-4f);
}

TEST_F(TestPositionProfile, a_new_target_mid_move_continues_from_the_reference_motion)
{
    Profile(20.0f, 400.0f);
    profile.SetTarget(foc::Radians{ 3.0f });
    const auto before = Run(60);

    profile.SetTarget(foc::Radians{ 0.0f });
    const auto after = Advance();

    EXPECT_NEAR(after.position.Value() - before.position.Value(), before.velocity.Value() * period, 0.5f * 400.0f * period * period + 1e-5f);
    EXPECT_NEAR(after.velocity.Value() - before.velocity.Value(), -400.0f * period, 1e-3f);
}

TEST_F(TestPositionProfile, a_reversal_mid_move_stays_inside_the_limits)
{
    Profile(20.0f, 400.0f, 300.0f);
    profile.SetTarget(foc::Radians{ 3.0f });
    auto previous = Run(60);
    profile.SetTarget(foc::Radians{ 0.0f });

    float peakSpeed{ 0.0f };
    float peakAcceleration{ 0.0f };
    for (std::size_t tick = 0; tick != 600; ++tick)
    {
        const auto reference = Advance();
        peakSpeed = std::max(peakSpeed, std::abs(reference.velocity.Value()));
        peakAcceleration = std::max(peakAcceleration, std::abs(reference.acceleration.Value()));
        EXPECT_NEAR(reference.position.Value() - previous.position.Value(), 0.5f * (reference.velocity.Value() + previous.velocity.Value()) * period, 2e-3f);
        previous = reference;
    }

    EXPECT_LE(peakSpeed, 20.0f + 1e-3f);
    EXPECT_LE(peakAcceleration, 400.0f + 1e-3f);
    EXPECT_NEAR(previous.position.Value(), 0.0f, 1e-4f);
}

TEST_F(TestPositionProfile, releasing_the_profile_reseeds_it_from_the_measured_state)
{
    Profile(20.0f, 400.0f);
    profile.SetTarget(foc::Radians{ 1.0f });
    Run(60);

    profile.Release();
    const auto reference = Advance(-1.0f, 0.0f);

    EXPECT_NEAR(reference.position.Value(), -1.0f + 0.5f * 400.0f * period * period, 1e-4f);
    EXPECT_NEAR(reference.velocity.Value(), 400.0f * period, 1e-3f);
}

TEST_F(TestPositionProfile, the_reseed_takes_the_measured_speed)
{
    Profile(20.0f, 400.0f);
    profile.SetTarget(foc::Radians{ 3.0f });

    const auto reference = Advance(0.0f, 10.0f);

    EXPECT_NEAR(reference.velocity.Value(), 10.0f + 400.0f * period, 1e-3f);
}

TEST_F(TestPositionProfile, the_reference_reports_the_target_until_the_profile_runs)
{
    Profile(20.0f, 400.0f);
    profile.SetTarget(foc::Radians{ 3.0f });

    EXPECT_EQ(profile.Reference().Value(), 3.0f);

    Run(10);

    EXPECT_LT(profile.Reference().Value(), 3.0f);
    EXPECT_EQ(profile.Target().Value(), 3.0f);
}

TEST_F(TestPositionProfile, switching_the_profile_off_returns_the_target)
{
    Profile(20.0f, 400.0f);
    profile.SetTarget(foc::Radians{ 3.0f });
    Run(10);

    Profile(0.0f, 0.0f);
    const auto reference = Advance();

    EXPECT_EQ(reference.position.Value(), 3.0f);
    EXPECT_EQ(reference.velocity.Value(), 0.0f);
}

TEST_F(TestPositionProfile, with_jerk_the_acceleration_ramps_smoothly)
{
    Profile(20.0f, 400.0f, 0.0f, 20000.0f);
    profile.SetTarget(foc::Radians{ 1.5f });

    float prevAcceleration{ 0.0f };
    float maxAccelerationStep{ 0.0f };
    for (std::size_t tick = 0; tick != 200; ++tick)
    {
        const auto reference = Advance();
        const float step = std::abs(reference.acceleration.Value() - prevAcceleration);
        maxAccelerationStep = std::max(maxAccelerationStep, step);
        prevAcceleration = reference.acceleration.Value();
    }

    EXPECT_LE(maxAccelerationStep, 20000.0f * period + 1.0f);
}

TEST_F(TestPositionProfile, with_jerk_zero_the_behaviour_is_identical_to_trapezoid)
{
    Profile(20.0f, 400.0f);
    profile.SetTarget(foc::Radians{ 1.5f });

    foc::PositionProfile trapezoidProfile{ outerFrequency };
    trapezoidProfile.Limit(foc::RadiansPerSecond{ 20.0f }, foc::RadiansPerSecondSquared{ 400.0f }, foc::RadiansPerSecondSquared{ 0.0f }, foc::RadiansPerSecondCubed{ 0.0f });
    trapezoidProfile.SetTarget(foc::Radians{ 1.5f });
    foc::TravelLimits noTravel;

    for (std::size_t tick = 0; tick != 200; ++tick)
    {
        const auto a = Advance();
        const auto b = trapezoidProfile.Advance(noTravel, foc::Radians{ 0.0f }, foc::RadiansPerSecond{ 0.0f });
        EXPECT_EQ(a.position.Value(), b.position.Value());
        EXPECT_EQ(a.velocity.Value(), b.velocity.Value());
    }
}

TEST_F(TestPositionProfile, with_jerk_a_mid_move_replan_changes_acceleration_by_at_most_one_jerk_step)
{
    Profile(20.0f, 400.0f, 0.0f, 20000.0f);
    profile.SetTarget(foc::Radians{ 3.0f });
    const auto before = Run(60);

    profile.SetTarget(foc::Radians{ 0.0f });
    const auto after = Advance();

    EXPECT_LE(std::abs(after.acceleration.Value() - before.acceleration.Value()), 20000.0f * period + 1.0f);
}

TEST_F(TestPositionProfile, with_jerk_the_move_reaches_the_target_in_about_145_ms)
{
    Profile(20.0f, 400.0f, 0.0f, 20000.0f);
    profile.SetTarget(foc::Radians{ 1.5f });

    std::size_t ticks{ 0 };
    foc::ProfiledReference reference;
    do
    {
        reference = Advance();
        ++ticks;
    } while (reference.position.Value() < 1.5f - 1e-4f && ticks < 400);

    EXPECT_NEAR(static_cast<float>(ticks) * period, 0.145f, 0.005f);
}

TEST_F(TestPositionProfile, reseed_with_jerk_clamps_initial_acceleration)
{
    Profile(20.0f, 400.0f, 0.0f, 20000.0f);
    profile.SetTarget(foc::Radians{ 1.5f });
    Run(50);

    Profile(20.0f, 200.0f, 0.0f, 20000.0f);
    profile.Release();

    EXPECT_NO_FATAL_FAILURE(Advance());
}
