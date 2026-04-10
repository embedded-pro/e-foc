#include "core/foc/implementations/TrigonometricImpl.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float lutTolerance = 0.015f;
    constexpr float exactTolerance = 1e-5f;

    class TestFastTrigonometry
        : public ::testing::Test
    {
    };

    class TestTrigonometricFunctions
        : public ::testing::Test
    {
    public:
        foc::TrigonometricFunctions trig;
    };
}

TEST_F(TestFastTrigonometry, sine_zero)
{
    EXPECT_NEAR(foc::FastTrigonometry::Sine(0.0f), 0.0f, lutTolerance);
}

TEST_F(TestFastTrigonometry, sine_pi_over_2)
{
    EXPECT_NEAR(foc::FastTrigonometry::Sine(pi / 2.0f), 1.0f, lutTolerance);
}

TEST_F(TestFastTrigonometry, sine_pi)
{
    EXPECT_NEAR(foc::FastTrigonometry::Sine(pi), 0.0f, lutTolerance);
}

TEST_F(TestFastTrigonometry, sine_three_pi_over_2)
{
    EXPECT_NEAR(foc::FastTrigonometry::Sine(3.0f * pi / 2.0f), -1.0f, lutTolerance);
}

TEST_F(TestFastTrigonometry, cosine_zero)
{
    EXPECT_NEAR(foc::FastTrigonometry::Cosine(0.0f), 1.0f, lutTolerance);
}

TEST_F(TestFastTrigonometry, cosine_pi_over_2)
{
    EXPECT_NEAR(foc::FastTrigonometry::Cosine(pi / 2.0f), 0.0f, lutTolerance);
}

TEST_F(TestFastTrigonometry, cosine_pi)
{
    EXPECT_NEAR(foc::FastTrigonometry::Cosine(pi), -1.0f, lutTolerance);
}

TEST_F(TestFastTrigonometry, sine_squared_plus_cosine_squared_equals_one)
{
    for (float angle = 0.0f; angle < 2.0f * pi; angle += 0.1f)
    {
        auto s = foc::FastTrigonometry::Sine(angle);
        auto c = foc::FastTrigonometry::Cosine(angle);
        EXPECT_NEAR(s * s + c * c, 1.0f, lutTolerance);
    }
}

TEST_F(TestFastTrigonometry, sine_matches_std_sin_across_range)
{
    for (float angle = 0.0f; angle < 2.0f * pi; angle += 0.05f)
    {
        EXPECT_NEAR(foc::FastTrigonometry::Sine(angle), std::sin(angle), lutTolerance)
            << "at angle " << angle;
    }
}

TEST_F(TestFastTrigonometry, cosine_matches_std_cos_across_range)
{
    for (float angle = 0.0f; angle < 2.0f * pi; angle += 0.05f)
    {
        EXPECT_NEAR(foc::FastTrigonometry::Cosine(angle), std::cos(angle), lutTolerance)
            << "at angle " << angle;
    }
}

TEST_F(TestFastTrigonometry, arctangent_zero)
{
    EXPECT_NEAR(foc::FastTrigonometry::Arctangent(0.0f), 0.0f, exactTolerance);
}

TEST_F(TestFastTrigonometry, arctangent_one)
{
    EXPECT_NEAR(foc::FastTrigonometry::Arctangent(1.0f), pi / 4.0f, exactTolerance);
}

TEST_F(TestFastTrigonometry, phase_positive_real_axis)
{
    EXPECT_NEAR(foc::FastTrigonometry::Phase(1.0f, 0.0f), 0.0f, exactTolerance);
}

TEST_F(TestFastTrigonometry, phase_positive_imaginary_axis)
{
    EXPECT_NEAR(foc::FastTrigonometry::Phase(0.0f, 1.0f), pi / 2.0f, exactTolerance);
}

TEST_F(TestFastTrigonometry, phase_negative_real_axis)
{
    EXPECT_NEAR(foc::FastTrigonometry::Phase(-1.0f, 0.0f), pi, exactTolerance);
}

TEST_F(TestTrigonometricFunctions, sine_delegates_to_fast_trigonometry)
{
    float angle = pi / 4.0f;
    EXPECT_NEAR(trig.Sine(angle), foc::FastTrigonometry::Sine(angle), exactTolerance);
}

TEST_F(TestTrigonometricFunctions, cosine_delegates_to_fast_trigonometry)
{
    float angle = pi / 3.0f;
    EXPECT_NEAR(trig.Cosine(angle), foc::FastTrigonometry::Cosine(angle), exactTolerance);
}

TEST_F(TestTrigonometricFunctions, arctangent_delegates_to_fast_trigonometry)
{
    float value = 0.5f;
    EXPECT_NEAR(trig.Arctangent(value), foc::FastTrigonometry::Arctangent(value), exactTolerance);
}

TEST_F(TestTrigonometricFunctions, phase_delegates_to_fast_trigonometry)
{
    float real = 1.0f;
    float imag = 1.0f;
    EXPECT_NEAR(trig.Phase(real, imag), foc::FastTrigonometry::Phase(real, imag), exactTolerance);
}
