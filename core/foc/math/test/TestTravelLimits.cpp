#include "core/foc/math/TravelLimits.hpp"
#include <gtest/gtest.h>
#include <numbers>

namespace
{
    constexpr float turn = 2.0f * std::numbers::pi_v<float>;
    const foc::TravelLimits limits{ foc::Radians{ -3.0f }, foc::Radians{ 3.0f } };
    const foc::TravelLimits circularLimits{ foc::Radians{ -3.0f }, foc::Radians{ 3.0f }, true };
    const foc::TravelLimits multiTurnLimits{ foc::Radians{ -2.0f * turn }, foc::Radians{ 3.0f * turn } };
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

TEST(TravelLimits, in_circular_mode_take_a_setpoint_modulo_a_turn)
{
    EXPECT_TRUE(circularLimits.Admits(foc::Radians{ 1.0f + turn }));
    EXPECT_FALSE(circularLimits.Admits(foc::Radians{ 3.1f + turn }));
    EXPECT_FALSE((foc::TravelLimits{ foc::Radians{ 0.0f }, foc::Radians{ 1.0f }, true }.Admits(foc::Radians{ 1.5f - turn })));
}

TEST(TravelLimits, in_linear_mode_take_a_setpoint_as_a_multi_turn_position)
{
    EXPECT_FALSE(limits.Admits(foc::Radians{ 1.0f + turn }));
    EXPECT_TRUE(multiTurnLimits.Admits(foc::Radians{ 2.5f * turn }));
    EXPECT_FALSE(multiTurnLimits.Admits(foc::Radians{ 3.1f * turn }));
    EXPECT_FALSE(multiTurnLimits.Admits(foc::Radians{ -2.1f * turn }));
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

TEST(TravelLimits, in_circular_mode_hold_a_reference_taken_modulo_a_turn)
{
    EXPECT_NEAR(1.0f, circularLimits.Hold(foc::Radians{ 1.0f - turn }).Value(), 1e-5f);
}

TEST(TravelLimits, in_linear_mode_hold_a_multi_turn_reference_without_folding_it)
{
    EXPECT_NEAR(-3.0f, limits.Hold(foc::Radians{ 1.0f - turn }).Value(), 1e-5f);
    EXPECT_NEAR(2.5f * turn, multiTurnLimits.Hold(foc::Radians{ 2.5f * turn }).Value(), 1e-4f);
    EXPECT_NEAR(3.0f * turn, multiTurnLimits.Hold(foc::Radians{ 4.0f * turn }).Value(), 1e-4f);
}

TEST(TravelLimits, fold_the_position_difference_only_in_circular_mode_without_limits)
{
    EXPECT_TRUE((foc::TravelLimits{ foc::Radians{ 0.0f }, foc::Radians{ 0.0f }, true }.Folds()));
    EXPECT_FALSE(circularLimits.Folds());
    EXPECT_FALSE(foc::TravelLimits{}.Folds());
    EXPECT_FALSE(limits.Folds());
}
