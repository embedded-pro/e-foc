#include "core/foc/interfaces/Units.hpp"
#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
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
        void Step()
        {
            model.StepForTest({ hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50) });
        }

        foc::ThreePhaseMotorModel model{
            foc::M_2310P_LN_04K::parameters,
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
    model.SetEncoderNoise(foc::ThreePhaseMotorModel::EncoderNoiseConfig{ 0.0f, bias });
    model.Set(foc::Radians{ 1.0f });

    for (int i = 0; i < 50; ++i)
        EXPECT_FLOAT_EQ(model.Read().Value(), 1.0f + bias);
}

TEST_F(TestEncoderNoise, sigma_yields_zero_mean_perturbation)
{
    constexpr float sigma = 0.05f;
    constexpr int samples = 4000;
    constexpr float baseAngle = 2.0f;

    model.SetEncoderNoise(foc::ThreePhaseMotorModel::EncoderNoiseConfig{ sigma, 0.0f });
    model.Set(foc::Radians{ baseAngle });

    double sum = 0.0;
    for (int i = 0; i < samples; ++i)
    {
        Step();
        sum += static_cast<double>(model.Read().Value()) - model.MechanicalAngle().Value();
    }

    const double mean = sum / samples;
    EXPECT_NEAR(mean, 0.0, 5.0 * sigma / std::sqrt(static_cast<double>(samples)));
}

TEST_F(TestEncoderNoise, noise_is_drawn_once_per_plant_step)
{
    model.SetEncoderNoise(foc::ThreePhaseMotorModel::EncoderNoiseConfig{ 0.05f, 0.0f });
    model.Set(foc::Radians{ 2.0f });

    Step();
    const auto first = model.Read().Value();
    EXPECT_FLOAT_EQ(model.Read().Value(), first);

    Step();
    EXPECT_NE(model.Read().Value(), first);
}

TEST_F(TestEncoderNoise, disabling_noise_takes_effect_before_the_next_step)
{
    model.SetEncoderNoise(foc::ThreePhaseMotorModel::EncoderNoiseConfig{ 0.05f, 0.0f });
    model.Set(foc::Radians{ 2.0f });
    Step();

    model.SetEncoderNoise(foc::ThreePhaseMotorModel::EncoderNoiseConfig{ 0.0f, 0.0f });

    EXPECT_FLOAT_EQ(model.Read().Value(), model.MechanicalAngle().Value());
}

TEST_F(TestEncoderNoise, output_is_wrapped_into_zero_to_two_pi)
{
    model.SetEncoderNoise(foc::ThreePhaseMotorModel::EncoderNoiseConfig{ 0.0f, 1.0f });
    model.Set(foc::Radians{ two_pi - 0.5f });

    const auto value = model.Read().Value();
    EXPECT_GE(value, 0.0f);
    EXPECT_LT(value, two_pi);
}
