#include "core/foc/model/EncoderModel.hpp"
#include <gtest/gtest.h>
#include <numbers>

namespace
{
    constexpr float countWidth = 2.0f * std::numbers::pi_v<float> / 4000.0f;

    class TestEncoderIndex
        : public ::testing::Test
    {
    protected:
        TestEncoderIndex()
        {
            encoder.SetResolution(4000);
        }

        foc::EncoderModel encoder;
    };
}

TEST_F(TestEncoderIndex, an_encoder_without_an_index_never_asserts_it)
{
    EXPECT_FALSE(encoder.HasIndex());
    EXPECT_FALSE(encoder.IndexAsserted(foc::Radians{ 0.0f }));
}

TEST_F(TestEncoderIndex, the_pulse_spans_its_width_after_the_index_angle)
{
    encoder.SetIndex(foc::Radians{ 2.5f }, 2);

    EXPECT_TRUE(encoder.HasIndex());
    EXPECT_FALSE(encoder.IndexAsserted(foc::Radians{ 2.5f - 0.5f * countWidth }));
    EXPECT_TRUE(encoder.IndexAsserted(foc::Radians{ 2.5f + 0.1f * countWidth }));
    EXPECT_TRUE(encoder.IndexAsserted(foc::Radians{ 2.5f + 1.9f * countWidth }));
    EXPECT_FALSE(encoder.IndexAsserted(foc::Radians{ 2.5f + 2.1f * countWidth }));
}

TEST_F(TestEncoderIndex, the_pulse_is_found_across_the_wrap_of_the_rotor_angle)
{
    encoder.SetIndex(foc::Radians{ -3.14f }, 4);

    EXPECT_TRUE(encoder.IndexAsserted(foc::Radians{ -3.14f + 2.0f * std::numbers::pi_v<float> + countWidth }));
}

TEST_F(TestEncoderIndex, setting_the_encoder_moves_its_zero_and_not_the_rotor)
{
    encoder.SetIndex(foc::Radians{ 0.0f }, 1);

    encoder.Set(foc::Radians{ 1.0f }, foc::Radians{ 0.25f });
    EXPECT_NEAR(encoder.Read(foc::Radians{ 0.25f }).Value(), 1.0f, countWidth);
    EXPECT_NEAR(encoder.Read(foc::Radians{ 0.75f }).Value(), 1.5f, countWidth);

    encoder.SetZero(foc::Radians{ 0.75f });
    EXPECT_NEAR(encoder.Read(foc::Radians{ 0.75f }).Value(), 0.0f, countWidth);
    EXPECT_NEAR(encoder.Read(foc::Radians{ 0.5f }).Value(), -0.25f, countWidth);
}
