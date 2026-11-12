#include "core/foc/transforms/DelayCompensation.hpp"
#include <cmath>
#include <gmock/gmock.h>

namespace
{
    constexpr float samplingFrequency = 20000.0f;

    struct Rotated
    {
        float cosine;
        float sine;
    };

    Rotated Exact(float theta, float advance)
    {
        return { std::cos(theta + advance), std::sin(theta + advance) };
    }

    foc::AngleRotation Advanced(float theta, float electricalSpeed, float delay)
    {
        return foc::AdvanceRotation(std::cos(theta), std::sin(theta), electricalSpeed, delay);
    }
}

TEST(TestDelayCompensation, the_delay_is_the_duty_delay_plus_the_half_period_hold)
{
    EXPECT_NEAR(foc::DutyDelay(0, samplingFrequency), 0.5f / samplingFrequency, 1e-9f);
    EXPECT_NEAR(foc::DutyDelay(1, samplingFrequency), 1.5f / samplingFrequency, 1e-9f);
}

TEST(TestDelayCompensation, an_unset_sampling_frequency_gives_no_delay)
{
    EXPECT_EQ(foc::DutyDelay(1, 0.0f), 0.0f);
}

TEST(TestDelayCompensation, zero_speed_returns_the_pair_unchanged)
{
    const auto rotated = Advanced(0.7f, 0.0f, foc::DutyDelay(1, samplingFrequency));

    EXPECT_EQ(rotated.cosine, std::cos(0.7f));
    EXPECT_EQ(rotated.sine, std::sin(0.7f));
}

TEST(TestDelayCompensation, no_delay_returns_the_pair_unchanged)
{
    const auto rotated = Advanced(2.1f, 3000.0f, 0.0f);

    EXPECT_EQ(rotated.cosine, std::cos(2.1f));
    EXPECT_EQ(rotated.sine, std::sin(2.1f));
}

TEST(TestDelayCompensation, the_rotation_matches_the_exact_angle_up_to_the_limit_of_the_series)
{
    const float delay = foc::DutyDelay(1, samplingFrequency);

    for (const float advance : { 0.01f, 0.05f, 0.1f, 0.19f, 0.3f, foc::maxDelayAdvance })
        for (const float theta : { -3.0f, -1.2f, 0.0f, 0.4f, 1.9f, 3.1f })
        {
            const auto rotated = Advanced(theta, advance / delay, delay);
            const auto exact = Exact(theta, advance);

            EXPECT_NEAR(rotated.cosine, exact.cosine, 7e-4f) << advance << " " << theta;
            EXPECT_NEAR(rotated.sine, exact.sine, 7e-4f) << advance << " " << theta;
        }
}

TEST(TestDelayCompensation, the_rotation_error_is_below_the_documented_bound_at_the_largest_advance)
{
    const float advance = foc::maxDelayAdvance;
    const float cosineError = std::pow(advance, 4.0f) / 24.0f;
    const float sineError = std::pow(advance, 5.0f) / 120.0f;

    EXPECT_LT(cosineError, 6.4e-4f);
    EXPECT_LT(sineError, 4.5e-5f);

    const auto rotated = foc::AdvanceRotation(1.0f, 0.0f, advance, 1.0f);
    EXPECT_NEAR(rotated.cosine, std::cos(advance), cosineError * 1.01f);
    EXPECT_NEAR(rotated.sine, std::sin(advance), sineError * 1.01f);
}

TEST(TestDelayCompensation, a_negative_speed_rotates_the_other_way)
{
    const float delay = foc::DutyDelay(1, samplingFrequency);
    const float speed = 2000.0f;

    const auto forward = Advanced(0.5f, speed, delay);
    const auto backward = Advanced(0.5f, -speed, delay);
    const auto exactForward = Exact(0.5f, speed * delay);
    const auto exactBackward = Exact(0.5f, -speed * delay);

    EXPECT_NEAR(forward.sine, exactForward.sine, 1e-4f);
    EXPECT_NEAR(backward.sine, exactBackward.sine, 1e-4f);
    EXPECT_LT(backward.sine, forward.sine);
}

TEST(TestDelayCompensation, a_speed_beyond_the_band_saturates_at_the_largest_advance)
{
    const float delay = foc::DutyDelay(1, samplingFrequency);

    const auto fast = Advanced(0.2f, 1e6f, delay);
    const auto limit = Advanced(0.2f, foc::maxDelayAdvance / delay, delay);
    const auto reverse = Advanced(0.2f, -1e6f, delay);
    const auto reverseLimit = Advanced(0.2f, -foc::maxDelayAdvance / delay, delay);

    EXPECT_NEAR(fast.cosine, limit.cosine, 1e-6f);
    EXPECT_NEAR(fast.sine, limit.sine, 1e-6f);
    EXPECT_NEAR(reverse.cosine, reverseLimit.cosine, 1e-6f);
    EXPECT_NEAR(reverse.sine, reverseLimit.sine, 1e-6f);
}

TEST(TestDelayCompensation, the_rotated_pair_keeps_unit_length_within_the_series_error)
{
    const float delay = foc::DutyDelay(1, samplingFrequency);

    for (const float speed : { -4000.0f, -2500.0f, 100.0f, 2513.0f, 4000.0f })
    {
        const auto rotated = Advanced(1.3f, speed, delay);
        EXPECT_NEAR(rotated.cosine * rotated.cosine + rotated.sine * rotated.sine, 1.0f, 1e-3f);
    }
}
