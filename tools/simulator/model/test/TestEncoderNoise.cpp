#include "core/foc/interfaces/Units.hpp"
#include "tools/simulator/model/Jk42bls01X038ed.hpp"
#include "tools/simulator/model/Model.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

namespace
{
    constexpr float two_pi = 2.0f * std::numbers::pi_v<float>;

    class TestEncoderNoise
        : public ::testing::Test
    {
    protected:
        simulator::ThreePhaseMotorModel model{
            simulator::JK42BLS01_X038ED::parameters,
            foc::Volts{ 24.0f },
            hal::Hertz{ 100000 },
            std::optional<std::size_t>{}
        };
    };
}

TEST_F(TestEncoderNoise, zero_config_returns_exact_angle)
{
    model.Set(foc::Radians{ 1.234f });
    EXPECT_FLOAT_EQ(model.Read().Value(), 1.234f);
}

TEST_F(TestEncoderNoise, bias_is_constant_offset)
{
    constexpr float bias = 0.25f;
    model.SetEncoderNoise(simulator::ThreePhaseMotorModel::EncoderNoiseConfig{ 0.0f, bias });
    model.Set(foc::Radians{ 1.0f });

    for (int i = 0; i < 50; ++i)
        EXPECT_FLOAT_EQ(model.Read().Value(), 1.0f + bias);
}

TEST_F(TestEncoderNoise, sigma_yields_zero_mean_perturbation)
{
    constexpr float sigma = 0.05f;
    constexpr int samples = 4000;
    constexpr float baseAngle = 2.0f;

    model.SetEncoderNoise(simulator::ThreePhaseMotorModel::EncoderNoiseConfig{ sigma, 0.0f });
    model.Set(foc::Radians{ baseAngle });

    double sum = 0.0;
    for (int i = 0; i < samples; ++i)
        sum += static_cast<double>(model.Read().Value()) - baseAngle;

    const double mean = sum / samples;
    EXPECT_NEAR(mean, 0.0, 5.0 * sigma / std::sqrt(static_cast<double>(samples)));
}

TEST_F(TestEncoderNoise, output_is_wrapped_into_zero_to_two_pi)
{
    model.SetEncoderNoise(simulator::ThreePhaseMotorModel::EncoderNoiseConfig{ 0.0f, 1.0f });
    model.Set(foc::Radians{ two_pi - 0.5f });

    const auto value = model.Read().Value();
    EXPECT_GE(value, 0.0f);
    EXPECT_LT(value, two_pi);
}
