#include "core/services/InjectionCurrentLimit.hpp"
#include <gmock/gmock.h>

namespace
{
    const foc::Ampere limit{ 10.0f };

    foc::PhaseCurrents Currents(float a, float b, float c)
    {
        return { foc::Ampere{ a }, foc::Ampere{ b }, foc::Ampere{ c } };
    }
}

TEST(TestInjectionCurrentLimit, all_phases_well_below_the_limit_is_not_exceeded)
{
    EXPECT_FALSE(services::ExceedsInjectionLimit(Currents(1.0f, -1.0f, 1.0f), limit));
}

TEST(TestInjectionCurrentLimit, exactly_at_the_limit_is_not_exceeded)
{
    EXPECT_FALSE(services::ExceedsInjectionLimit(Currents(10.0f, 0.0f, 0.0f), limit));
}

TEST(TestInjectionCurrentLimit, one_step_under_the_limit_is_not_exceeded)
{
    EXPECT_FALSE(services::ExceedsInjectionLimit(Currents(9.999f, 0.0f, 0.0f), limit));
}

TEST(TestInjectionCurrentLimit, one_step_over_the_limit_on_phase_a_is_exceeded)
{
    EXPECT_TRUE(services::ExceedsInjectionLimit(Currents(10.001f, 0.0f, 0.0f), limit));
}

TEST(TestInjectionCurrentLimit, one_step_over_the_limit_on_phase_b_is_exceeded)
{
    EXPECT_TRUE(services::ExceedsInjectionLimit(Currents(0.0f, 10.001f, 0.0f), limit));
}

TEST(TestInjectionCurrentLimit, one_step_over_the_limit_on_phase_c_is_exceeded)
{
    EXPECT_TRUE(services::ExceedsInjectionLimit(Currents(0.0f, 0.0f, 10.001f), limit));
}

TEST(TestInjectionCurrentLimit, a_negative_current_over_the_limit_in_magnitude_is_exceeded)
{
    EXPECT_TRUE(services::ExceedsInjectionLimit(Currents(-10.001f, 0.0f, 0.0f), limit));
}

TEST(TestInjectionCurrentLimit, a_single_phase_over_the_limit_is_exceeded_even_when_the_others_are_within_it)
{
    EXPECT_TRUE(services::ExceedsInjectionLimit(Currents(1.0f, -1.0f, 10.5f), limit));
}
