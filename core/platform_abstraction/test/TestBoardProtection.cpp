#include "core/platform_abstraction/BoardProtection.hpp"
#include <gmock/gmock.h>
#include <limits>

namespace
{
    using State = application::PlatformFactory::BoardProtectionState;

    class TestBoardProtection
        : public ::testing::Test
    {
    public:
        State Evaluate(bool overCurrentLatched, bool pwmFaultLatched, float busVolts)
        {
            return application::EvaluateBoardProtection(overCurrentLatched, pwmFaultLatched, limits, [this, busVolts]()
                {
                    ++busReads;
                    return foc::Volts{ busVolts };
                });
        }

        const application::BoardProtectionLimits limits{ foc::Volts{ 58.0f }, foc::Volts{ 18.0f } };
        uint32_t busReads{ 0 };
    };
}

TEST_F(TestBoardProtection, no_latch_and_a_bus_inside_the_thresholds_is_clear)
{
    EXPECT_EQ(Evaluate(false, false, 24.0f), State::clear);
    EXPECT_EQ(busReads, 1u);
}

TEST_F(TestBoardProtection, a_bus_exactly_at_a_threshold_is_clear)
{
    EXPECT_EQ(Evaluate(false, false, 58.0f), State::clear);
    EXPECT_EQ(Evaluate(false, false, 18.0f), State::clear);
}

TEST_F(TestBoardProtection, a_bus_above_the_overvoltage_threshold_is_asserted)
{
    EXPECT_EQ(Evaluate(false, false, 58.5f), State::asserted);
}

TEST_F(TestBoardProtection, a_bus_below_the_undervoltage_threshold_is_asserted)
{
    EXPECT_EQ(Evaluate(false, false, 17.5f), State::asserted);
}

TEST_F(TestBoardProtection, a_bus_reading_that_is_not_a_number_is_asserted)
{
    EXPECT_EQ(Evaluate(false, false, std::numeric_limits<float>::quiet_NaN()), State::asserted);
}

TEST_F(TestBoardProtection, the_over_current_latch_asserts_without_reading_the_bus)
{
    EXPECT_EQ(Evaluate(true, false, 24.0f), State::asserted);
    EXPECT_EQ(busReads, 0u);
}

TEST_F(TestBoardProtection, the_pwm_fault_latch_asserts_without_reading_the_bus)
{
    EXPECT_EQ(Evaluate(false, true, 24.0f), State::asserted);
    EXPECT_EQ(busReads, 0u);
}

TEST(TestExceedsOverCurrent, any_phase_beyond_the_limit_in_either_direction_exceeds_it)
{
    const foc::Ampere limit{ 6.6f };

    EXPECT_FALSE(application::ExceedsOverCurrent(foc::Ampere{ 6.6f }, foc::Ampere{ -6.6f }, foc::Ampere{ 0.0f }, limit));
    EXPECT_TRUE(application::ExceedsOverCurrent(foc::Ampere{ 6.7f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, limit));
    EXPECT_TRUE(application::ExceedsOverCurrent(foc::Ampere{ 0.0f }, foc::Ampere{ -6.7f }, foc::Ampere{ 0.0f }, limit));
    EXPECT_TRUE(application::ExceedsOverCurrent(foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 7.0f }, limit));
}
