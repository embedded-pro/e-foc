#include "core/platform_abstraction/EncoderZero.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float degree = pi / 180.0f;

    class TestEncoderZero
        : public ::testing::Test
    {
    public:
        float AngleAt(float reading) const
        {
            return zero.AngleOf(foc::Radians{ reading }).Value();
        }

        application::EncoderZero zero;
    };
}

TEST_F(TestEncoderZero, reads_the_encoder_unchanged_until_a_zero_is_set)
{
    EXPECT_NEAR(AngleAt(0.0f), 0.0f, 1e-6f);
    EXPECT_NEAR(AngleAt(-pi), -pi, 1e-6f);
    EXPECT_NEAR(AngleAt(3.0f), 3.0f, 1e-6f);
}

TEST_F(TestEncoderZero, the_reading_at_the_zero_reads_as_zero)
{
    zero.SetZero(foc::Radians{ 1.2f });

    EXPECT_NEAR(AngleAt(1.2f), 0.0f, 1e-6f);
    EXPECT_NEAR(AngleAt(1.7f), 0.5f, 1e-6f);
}

TEST_F(TestEncoderZero, a_target_past_the_encoder_seam_reads_within_half_a_turn_of_the_zero)
{
    zero.SetZero(foc::Radians{ 100.0f * degree });

    EXPECT_NEAR(AngleAt(-170.0f * degree), 90.0f * degree, 1e-5f);
    EXPECT_NEAR(AngleAt(-90.0f * degree), 170.0f * degree, 1e-5f);
}

TEST_F(TestEncoderZero, a_zero_behind_the_seam_wraps_the_other_way)
{
    zero.SetZero(foc::Radians{ -100.0f * degree });

    EXPECT_NEAR(AngleAt(170.0f * degree), -90.0f * degree, 1e-5f);
    EXPECT_NEAR(AngleAt(90.0f * degree), -170.0f * degree, 1e-5f);
}

TEST_F(TestEncoderZero, every_reading_from_any_zero_lands_in_half_a_turn_either_side)
{
    for (int32_t zeroDegrees = -180; zeroDegrees < 180; zeroDegrees += 15)
    {
        zero.SetZero(foc::Radians{ static_cast<float>(zeroDegrees) * degree });

        for (int32_t readingDegrees = -180; readingDegrees < 180; readingDegrees += 5)
        {
            const auto reading = static_cast<float>(readingDegrees) * degree;
            const auto angle = AngleAt(reading);
            const auto turns = (angle - (reading - static_cast<float>(zeroDegrees) * degree)) / (2.0f * pi);

            EXPECT_GE(angle, -pi) << "zero " << zeroDegrees << ", reading " << readingDegrees;
            EXPECT_LT(angle, pi) << "zero " << zeroDegrees << ", reading " << readingDegrees;
            EXPECT_NEAR(turns, std::round(turns), 1e-5f) << "zero " << zeroDegrees << ", reading " << readingDegrees;
        }
    }
}

TEST_F(TestEncoderZero, set_makes_the_current_reading_read_as_the_given_angle_wrapped_to_one_turn)
{
    zero.Set(foc::Radians{ 0.5f }, foc::Radians{ 1.0f });
    EXPECT_NEAR(AngleAt(0.5f), 1.0f, 1e-6f);

    zero.Set(foc::Radians{ 0.5f }, foc::Radians{ 1.5f * pi });
    EXPECT_NEAR(AngleAt(0.5f), -0.5f * pi, 1e-5f);

    zero.Set(foc::Radians{ -3.0f }, foc::Radians{ -2.0f * pi - 0.5f });
    EXPECT_NEAR(AngleAt(-3.0f), -0.5f, 1e-5f);
}
