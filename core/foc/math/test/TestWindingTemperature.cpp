#include "core/foc/math/WindingTemperature.hpp"
#include <cmath>
#include <gtest/gtest.h>

TEST(WindingTemperature, equals_the_calibration_temperature_at_the_calibrated_resistance)
{
    const auto temperature = foc::WindingTemperatureFromResistance(foc::Ohm{ 1.1f }, foc::Ohm{ 1.1f }, foc::Celsius{ 25.0f });

    ASSERT_TRUE(temperature.has_value());
    EXPECT_NEAR(25.0f, temperature->Value(), 1e-4f);
}

TEST(WindingTemperature, rises_by_one_kelvin_per_0_393_percent_of_resistance)
{
    const auto temperature = foc::WindingTemperatureFromResistance(foc::Ohm{ 1.1f * (1.0f + 0.00393f * 35.0f) }, foc::Ohm{ 1.1f }, foc::Celsius{ 25.0f });

    ASSERT_TRUE(temperature.has_value());
    EXPECT_NEAR(60.0f, temperature->Value(), 1e-3f);
}

TEST(WindingTemperature, starts_from_the_stored_calibration_temperature)
{
    const auto temperature = foc::WindingTemperatureFromResistance(foc::Ohm{ 1.0f }, foc::Ohm{ 1.0f }, foc::Celsius{ 40.0f });

    ASSERT_TRUE(temperature.has_value());
    EXPECT_NEAR(40.0f, temperature->Value(), 1e-4f);
}

TEST(WindingTemperature, is_unknown_without_a_calibrated_resistance)
{
    EXPECT_FALSE(foc::WindingTemperatureFromResistance(foc::Ohm{ 1.0f }, foc::Ohm{ 0.0f }, foc::Celsius{ 25.0f }).has_value());
}

TEST(WindingTemperature, is_unknown_without_a_usable_online_resistance)
{
    EXPECT_FALSE(foc::WindingTemperatureFromResistance(foc::Ohm{ 0.0f }, foc::Ohm{ 1.0f }, foc::Celsius{ 25.0f }).has_value());
    EXPECT_FALSE(foc::WindingTemperatureFromResistance(foc::Ohm{ std::nanf("") }, foc::Ohm{ 1.0f }, foc::Celsius{ 25.0f }).has_value());
}
