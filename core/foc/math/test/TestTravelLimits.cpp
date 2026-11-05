#include "core/foc/math/TravelLimits.hpp"
#include <gtest/gtest.h>
#include <numbers>

namespace
{
    const foc::TravelLimits limits{ foc::Radians{ -3.0f }, foc::Radians{ 3.0f } };
}

TEST(TravelLimits, are_off_while_the_minimum_is_not_below_the_maximum)
{
    EXPECT_FALSE(foc::TravelLimits{}.IsOn());
    EXPECT_FALSE((foc::TravelLimits{ foc::Radians{ 1.0f }, foc::Radians{ 1.0f } }.IsOn()));
    EXPECT_FALSE((foc::TravelLimits{ foc::Radians{ 1.0f }, foc::Radians{ -1.0f } }.IsOn()));
    EXPECT_TRUE(limits.IsOn());
}

TEST(TravelLimits, admit_a_setpoint_between_them_and_on_them)
{
    EXPECT_TRUE(limits.Admits(foc::Radians{ 0.0f }));
    EXPECT_TRUE(limits.Admits(foc::Radians{ -3.0f }));
    EXPECT_TRUE(limits.Admits(foc::Radians{ 3.0f }));
}

TEST(TravelLimits, refuse_a_setpoint_beyond_them)
{
    EXPECT_FALSE(limits.Admits(foc::Radians{ 3.1f }));
    EXPECT_FALSE(limits.Admits(foc::Radians{ -3.1f }));
}

TEST(TravelLimits, take_a_setpoint_modulo_a_turn)
{
    constexpr float turn = 2.0f * std::numbers::pi_v<float>;

    EXPECT_TRUE(limits.Admits(foc::Radians{ 1.0f + turn }));
    EXPECT_FALSE(limits.Admits(foc::Radians{ 3.1f + turn }));
    EXPECT_FALSE((foc::TravelLimits{ foc::Radians{ 0.0f }, foc::Radians{ 1.0f } }.Admits(foc::Radians{ 1.5f - turn })));
}

TEST(TravelLimits, that_are_off_admit_every_setpoint)
{
    EXPECT_TRUE(foc::TravelLimits{}.Admits(foc::Radians{ 3.1f }));
}

TEST(TravelLimits, hold_a_reference_within_them)
{
    EXPECT_NEAR(3.0f, limits.Hold(foc::Radians{ 3.1f }).Value(), 1e-6f);
    EXPECT_NEAR(-3.0f, limits.Hold(foc::Radians{ -3.1f }).Value(), 1e-6f);
    EXPECT_NEAR(1.0f, limits.Hold(foc::Radians{ 1.0f }).Value(), 1e-6f);
}

TEST(TravelLimits, hold_a_reference_taken_modulo_a_turn)
{
    constexpr float turn = 2.0f * std::numbers::pi_v<float>;

    EXPECT_NEAR(1.0f, limits.Hold(foc::Radians{ 1.0f - turn }).Value(), 1e-5f);
}
