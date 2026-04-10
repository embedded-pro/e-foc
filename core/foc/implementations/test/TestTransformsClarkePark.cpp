#include "numerical/math/Tolerance.hpp"
#include "core/foc/implementations/TransformsClarkePark.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr auto pi = std::numbers::pi_v<float>;
    constexpr auto pi_2 = pi / 2.0f;
    constexpr auto pi_4 = pi / 4.0f;

    foc::ThreePhase CreateThreePhase(float a, float b, float c)
    {
        return { a, b, c };
    }

    foc::TwoPhase CreateTwoPhase(float alpha, float beta)
    {
        return { alpha, beta };
    }

    class TestTransforms : public ::testing::Test
    {
    public:
        std::optional<foc::Clarke> clarke;
        std::optional<foc::Park> park;
        std::optional<foc::ClarkePark> clarkePark;

        void SetUp() override
        {
            clarke.emplace();
            park.emplace();
            clarkePark.emplace();
        }
    };
}

TEST_F(TestTransforms, clarke_balanced_system)
{
    auto input = CreateThreePhase(0.5f, -0.25f, -0.25f);
    auto result = clarke->Forward(input);

    float tolerance = math::Tolerance<float>();

    EXPECT_NEAR(result.alpha, 0.5f, tolerance);
    EXPECT_NEAR(result.beta, 0.0f, tolerance);
}

TEST_F(TestTransforms, clarke_zero_input)
{
    auto input = CreateThreePhase(0.0f, 0.0f, 0.0f);
    auto result = clarke->Forward(input);

    float tolerance = math::Tolerance<float>();

    EXPECT_NEAR(result.alpha, 0.0f, tolerance);
    EXPECT_NEAR(result.beta, 0.0f, tolerance);
}

TEST_F(TestTransforms, clarke_inverse_recovers_original)
{
    auto input = CreateThreePhase(0.5f, -0.2f, -0.3f);
    auto alphabeta = clarke->Forward(input);
    auto result = clarke->Inverse(alphabeta);

    float tolerance = math::Tolerance<float>();

    EXPECT_NEAR(result.a, input.a, tolerance);
    EXPECT_NEAR(result.b, input.b, tolerance);
    EXPECT_NEAR(result.c, input.c, tolerance);
}

TEST_F(TestTransforms, park_zero_angle)
{
    auto cos = std::cos(0.0f);
    auto sin = std::sin(0.0f);
    auto input = CreateTwoPhase(0.3f, 0.0f);
    auto result = park->Forward(input, cos, sin);

    float tolerance = math::Tolerance<float>();

    EXPECT_NEAR(result.d, input.alpha, tolerance);
    EXPECT_NEAR(result.q, 0.0f, tolerance);
}

TEST_F(TestTransforms, park_ninety_degrees)
{
    auto cos = std::cos(pi_2);
    auto sin = std::sin(pi_2);
    auto input = CreateTwoPhase(0.1f, 0.0f);
    auto result = park->Forward(input, cos, sin);

    float tolerance = math::Tolerance<float>();

    EXPECT_NEAR(result.d, 0.0f, tolerance);
    EXPECT_NEAR(result.q, -input.alpha, tolerance);
}

TEST_F(TestTransforms, park_inverse_recovers_original)
{
    auto cos = std::cos(pi_4);
    auto sin = std::sin(pi_4);
    auto input = CreateTwoPhase(0.5f, 0.3f);

    auto dq = park->Forward(input, cos, sin);
    auto result = park->Inverse(dq, cos, sin);

    float tolerance = math::Tolerance<float>();

    EXPECT_NEAR(result.alpha, input.alpha, tolerance);
    EXPECT_NEAR(result.beta, input.beta, tolerance);
}

TEST_F(TestTransforms, clarke_park_full_transform)
{
    auto cos = std::cos(pi / 6.0f);
    auto sin = std::sin(pi / 6.0f);
    auto input = CreateThreePhase(0.4f, -0.2f, -0.2f);

    auto dq = clarkePark->Forward(input, cos, sin);
    auto result = clarkePark->Inverse(dq, cos, sin);

    float tolerance = math::Tolerance<float>();

    EXPECT_NEAR(result.a, input.a, tolerance);
    EXPECT_NEAR(result.b, input.b, tolerance);
    EXPECT_NEAR(result.c, input.c, tolerance);
}

TEST_F(TestTransforms, clarke_park_multiple_angles)
{
    auto input = CreateThreePhase(0.5f, -0.25f, -0.25f);
    std::vector<float> angles = { 0.0f, pi_4, pi_2, 3.0f * pi_4, pi };

    float tolerance = math::Tolerance<float>();

    for (const auto& angle : angles)
    {
        auto cos = std::cos(angle);
        auto sin = std::sin(angle);

        auto dq = clarkePark->Forward(input, cos, sin);
        auto result = clarkePark->Inverse(dq, cos, sin);

        EXPECT_NEAR(result.a, input.a, tolerance);
        EXPECT_NEAR(result.b, input.b, tolerance);
        EXPECT_NEAR(result.c, input.c, tolerance);
    }
}

TEST_F(TestTransforms, clarke_unbalanced_system)
{
    auto input = CreateThreePhase(0.4f, -0.05f, -0.2f);
    auto result = clarke->Forward(input);

    float tolerance = math::Tolerance<float>();

    EXPECT_NEAR(result.alpha, 0.35f, tolerance);
    EXPECT_NEAR(result.beta, 0.0866f, tolerance);
}

TEST_F(TestTransforms, park_negative_angles)
{
    auto cos = std::cos(-pi_4);
    auto sin = std::sin(-pi_4);

    auto input = CreateTwoPhase(0.3f, 0.2f);
    auto result = park->Forward(input, cos, sin);

    float tolerance = math::Tolerance<float>();

    float cos45 = std::cos(-pi_4);
    float sin45 = std::sin(-pi_4);

    EXPECT_NEAR(result.d, 0.3f * cos45 + 0.2f * sin45, tolerance);
    EXPECT_NEAR(result.q, -0.3f * sin45 + 0.2f * cos45, tolerance);
}

TEST_F(TestTransforms, clarke_park_near_limits)
{
    auto cos = std::cos(pi / 3.0f);
    auto sin = std::sin(pi / 3.0f);

    float max_val = 0.5f;
    auto input = CreateThreePhase(max_val, -max_val / 2, -max_val / 2);

    auto dq = clarkePark->Forward(input, cos, sin);
    auto result = clarkePark->Inverse(dq, cos, sin);

    float tolerance = math::Tolerance<float>();

    EXPECT_NEAR(result.a, input.a, tolerance);
    EXPECT_NEAR(result.b, input.b, tolerance);
    EXPECT_NEAR(result.c, input.c, tolerance);
}

TEST_F(TestTransforms, clarke_dc_offset)
{
    auto input = CreateThreePhase(0.6f, 0.1f, 0.1f);
    auto result = clarke->Forward(input);

    float tolerance = math::Tolerance<float>();

    EXPECT_NEAR(result.alpha, 2.0f / 3.0f * (0.6f - 0.1f), tolerance);
    EXPECT_NEAR(result.beta, 0.0f, tolerance);
}

TEST_F(TestTransforms, park_harmonic_angle)
{
    float base_rads = pi / 6.0f;
    float harmonic_rads = std::fmod(base_rads + 2.0f * pi, 2.0f * pi);

    auto base_cos = std::cos(base_rads);
    auto base_sin = std::sin(base_rads);

    float harmonic_cos = std::cos(harmonic_rads);
    float harmonic_sin = std::sin(harmonic_rads);

    auto input = CreateTwoPhase(0.2f, 0.3f);
    float tolerance = math::Tolerance<float>();

    auto result1 = park->Forward(input, base_sin, base_cos);
    auto result2 = park->Forward(input, harmonic_sin, harmonic_cos);

    EXPECT_NEAR(result1.d, result2.d, tolerance);
    EXPECT_NEAR(result1.q, result2.q, tolerance);
}

TEST_F(TestTransforms, clarke_park_small_values)
{
    float small_val = 0.001f;
    float rel_tolerance = 0.05f;

    auto input = CreateThreePhase(small_val, -small_val / 2, -small_val / 2);

    auto cos = std::cos(pi_4);
    auto sin = std::sin(pi_4);

    auto dq = clarkePark->Forward(input, cos, sin);
    auto result = clarkePark->Inverse(dq, cos, sin);

    auto IsErrorAcceptable = [rel_tolerance](float expected, float actual, const char* label) -> bool
    {
        float abs_diff = std::abs(actual - expected);
        float abs_expected = std::abs(expected);

        if (abs_expected < 1e-4f)
            return abs_diff < 1e-4f;

        float rel_error = abs_diff / abs_expected;
        return rel_error <= rel_tolerance;
    };

    EXPECT_TRUE(IsErrorAcceptable(input.a, result.a, "a value"));
    EXPECT_TRUE(IsErrorAcceptable(input.b, result.b, "b value"));
    EXPECT_TRUE(IsErrorAcceptable(input.c, result.c, "c value"));
}
