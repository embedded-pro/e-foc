#include "tools/virtual_motor/platform/TriggerWatch.hpp"
#include "gtest/gtest.h"
#include <limits>

namespace
{
    using namespace virtual_motor::port;

    constexpr uint8_t speed{ 18 };

    Sample WithSpeed(float value)
    {
        Sample sample{};
        sample[speed] = value;
        return sample;
    }

    class TestTriggerWatch
        : public testing::Test
    {
    public:
        bool Fires(float value, uint8_t leds = 0, bool protection = false)
        {
            return watch.Fires(WithSpeed(value), leds, protection);
        }

        virtual_motor::TriggerWatch watch;
    };
}

TEST_F(TestTriggerWatch, immediately_fires_in_the_first_period_watched)
{
    watch.Arm(Trigger{ TriggerKind::immediately });

    EXPECT_TRUE(Fires(0.0f));
}

TEST_F(TestTriggerWatch, rising_fires_when_the_signal_reaches_the_level_from_below)
{
    watch.Arm(Trigger{ TriggerKind::rising, speed, 10.0f });

    EXPECT_FALSE(Fires(5.0f));
    EXPECT_FALSE(Fires(9.9f));
    EXPECT_TRUE(Fires(10.0f));
}

TEST_F(TestTriggerWatch, falling_fires_when_the_signal_reaches_the_level_from_above)
{
    watch.Arm(Trigger{ TriggerKind::falling, speed, 10.0f });

    EXPECT_FALSE(Fires(5.0f));
    EXPECT_FALSE(Fires(12.0f));
    EXPECT_TRUE(Fires(9.0f));
}

TEST_F(TestTriggerWatch, either_fires_on_a_crossing_in_each_direction)
{
    watch.Arm(Trigger{ TriggerKind::either, speed, 10.0f });
    EXPECT_FALSE(Fires(12.0f));
    EXPECT_TRUE(Fires(8.0f));

    watch.Arm(Trigger{ TriggerKind::either, speed, 10.0f });
    EXPECT_FALSE(Fires(8.0f));
    EXPECT_TRUE(Fires(12.0f));
}

TEST_F(TestTriggerWatch, a_level_already_crossed_when_armed_does_not_fire)
{
    watch.Arm(Trigger{ TriggerKind::rising, speed, 10.0f });

    EXPECT_FALSE(Fires(15.0f));
    EXPECT_FALSE(Fires(16.0f));
}

TEST_F(TestTriggerWatch, protection_fires_when_it_becomes_asserted)
{
    watch.Arm(Trigger{ TriggerKind::protection });
    EXPECT_FALSE(Fires(0.0f, 0, true));
    EXPECT_FALSE(Fires(0.0f, 0, false));
    EXPECT_TRUE(Fires(0.0f, 0, true));
}

TEST_F(TestTriggerWatch, leds_fire_when_an_led_of_the_mask_lights)
{
    watch.Arm(Trigger{ TriggerKind::leds, 0, 0.0f, led::failure | led::warning });

    EXPECT_FALSE(Fires(0.0f, led::operational));
    EXPECT_TRUE(Fires(0.0f, led::operational | led::warning));
}

TEST_F(TestTriggerWatch, an_led_already_lit_when_armed_does_not_fire)
{
    watch.Arm(Trigger{ TriggerKind::leds, 0, 0.0f, led::failure });

    EXPECT_FALSE(Fires(0.0f, led::failure));
    EXPECT_FALSE(Fires(0.0f, led::failure));
}

TEST_F(TestTriggerWatch, a_trigger_fires_once)
{
    watch.Arm(Trigger{ TriggerKind::immediately });

    EXPECT_TRUE(Fires(0.0f));
    EXPECT_FALSE(Fires(0.0f));
}

TEST(TestTriggerCheck, refuses_an_unknown_kind_a_signal_outside_the_data_model_or_a_level_that_is_not_finite)
{
    EXPECT_FALSE(virtual_motor::TriggerWatch::Check(Trigger{ TriggerKind::rising, 24, 1.0f }).has_value());
    EXPECT_EQ(virtual_motor::TriggerWatch::Check(Trigger{ static_cast<TriggerKind>(9) }), RefusalReason::outOfRange);
    EXPECT_EQ(virtual_motor::TriggerWatch::Check(Trigger{ TriggerKind::rising, 25, 1.0f }), RefusalReason::outOfRange);
    EXPECT_EQ(virtual_motor::TriggerWatch::Check(Trigger{ TriggerKind::falling, 0, std::numeric_limits<float>::infinity() }), RefusalReason::outOfRange);
}
