#include "core/services/flux_linkage_ident/BackEmfFit.hpp"
#include <cmath>
#include <gtest/gtest.h>

namespace
{
    constexpr float fluxLinkage = 0.0046f;

    class TestBackEmfFit
        : public ::testing::Test
    {
    public:
        void AddLine(float offset, float noise = 0.0f)
        {
            for (int sample = 0; sample < 200; ++sample)
            {
                const float speed = 40.0f + 0.6f * static_cast<float>(sample);
                const float sign = (sample % 2 == 0) ? 1.0f : -1.0f;
                fit.Add(speed, fluxLinkage * speed + offset + sign * noise);
            }
        }

        services::BackEmfFit fit;
    };
}

TEST_F(TestBackEmfFit, an_exact_line_gives_its_slope_and_intercept)
{
    AddLine(0.05f);

    EXPECT_NEAR(fit.Slope(), fluxLinkage, 1e-6f);
    EXPECT_NEAR(fit.Intercept(), 0.05f, 1e-4f);
    EXPECT_NEAR(fit.RelativeSlopeError(), 0.0f, 1e-3f);
}

TEST_F(TestBackEmfFit, noise_widens_the_slope_error)
{
    AddLine(0.0f, 0.05f);

    EXPECT_NEAR(fit.Slope(), fluxLinkage, 5e-5f);
    EXPECT_GT(fit.RelativeSlopeError(), 0.005f);
}

TEST_F(TestBackEmfFit, tracks_the_speed_span_and_the_largest_voltage)
{
    AddLine(0.0f);

    EXPECT_EQ(fit.Count(), 200u);
    EXPECT_NEAR(fit.LowestSpeed(), 40.0f, 1e-3f);
    EXPECT_NEAR(fit.HighestSpeed(), 159.4f, 1e-3f);
    EXPECT_NEAR(fit.LargestVoltage(), fluxLinkage * 159.4f, 1e-4f);
}

TEST_F(TestBackEmfFit, a_single_speed_has_no_slope)
{
    for (int sample = 0; sample < 10; ++sample)
        fit.Add(80.0f, 0.4f);

    EXPECT_EQ(fit.Slope(), 0.0f);
    EXPECT_TRUE(std::isinf(fit.RelativeSlopeError()));
}
