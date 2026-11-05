#include "core/foc/cascade/BusVoltageRatio.hpp"
#include <gtest/gtest.h>
#include <limits>

TEST(TestBusVoltageRatio, without_a_measurement_the_ratio_is_one)
{
    foc::BusVoltageRatio ratio;
    ratio.Configure(foc::Volts{ 24.0f });

    EXPECT_FLOAT_EQ(1.0f, ratio.Ratio());
    EXPECT_FLOAT_EQ(1.0f, ratio.Inverse());
}

TEST(TestBusVoltageRatio, a_measured_bus_sets_the_ratio_to_the_configured_one_and_its_inverse)
{
    foc::BusVoltageRatio ratio;
    ratio.Configure(foc::Volts{ 24.0f });

    ratio.Track(foc::Volts{ 18.0f });

    EXPECT_NEAR(0.75f, ratio.Ratio(), 1e-6f);
    EXPECT_NEAR(1.0f / 0.75f, ratio.Inverse(), 1e-5f);
}

TEST(TestBusVoltageRatio, the_ratio_is_held_within_half_and_twice)
{
    foc::BusVoltageRatio ratio;
    ratio.Configure(foc::Volts{ 24.0f });

    ratio.Track(foc::Volts{ 6.0f });
    EXPECT_FLOAT_EQ(0.5f, ratio.Ratio());
    EXPECT_FLOAT_EQ(2.0f, ratio.Inverse());

    ratio.Track(foc::Volts{ 96.0f });
    EXPECT_FLOAT_EQ(2.0f, ratio.Ratio());
    EXPECT_FLOAT_EQ(0.5f, ratio.Inverse());
}

TEST(TestBusVoltageRatio, a_measurement_that_is_not_a_number_keeps_the_ratio)
{
    foc::BusVoltageRatio ratio;
    ratio.Configure(foc::Volts{ 24.0f });
    ratio.Track(foc::Volts{ 18.0f });

    ratio.Track(foc::Volts{ std::numeric_limits<float>::quiet_NaN() });

    EXPECT_NEAR(0.75f, ratio.Ratio(), 1e-6f);
}

TEST(TestBusVoltageRatio, a_measurement_before_the_configuration_is_ignored)
{
    foc::BusVoltageRatio ratio;

    ratio.Track(foc::Volts{ 12.0f });

    EXPECT_FLOAT_EQ(1.0f, ratio.Ratio());
}

TEST(TestBusVoltageRatio, a_new_configuration_starts_again_from_one)
{
    foc::BusVoltageRatio ratio;
    ratio.Configure(foc::Volts{ 24.0f });
    ratio.Track(foc::Volts{ 18.0f });

    ratio.Configure(foc::Volts{ 36.0f });

    EXPECT_FLOAT_EQ(1.0f, ratio.Ratio());
    EXPECT_FLOAT_EQ(1.0f, ratio.Inverse());
}
