#include "integration_tests/support/response/StepMetrics.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

namespace
{
    using namespace integration::response;

    constexpr float dt = 0.001f;

    class StepMetricsTest
        : public ::testing::Test
    {
    protected:
        std::vector<float> FirstOrder(float magnitude, float tau, std::size_t count) const
        {
            std::vector<float> y;
            for (std::size_t i = 0; i != count; ++i)
                y.push_back(magnitude * (1.0f - std::exp(-static_cast<float>(i) * dt / tau)));
            return y;
        }

        std::vector<float> SecondOrder(float magnitude, float omegaN, float zeta, std::size_t count) const
        {
            std::vector<float> y;
            const float omegaD = omegaN * std::sqrt(1.0f - zeta * zeta);
            const float phase = std::acos(zeta);
            for (std::size_t i = 0; i != count; ++i)
            {
                const float t = static_cast<float>(i) * dt;
                const float envelope = std::exp(-zeta * omegaN * t) / std::sqrt(1.0f - zeta * zeta);
                y.push_back(magnitude * (1.0f - envelope * std::sin(omegaD * t + phase)));
            }
            return y;
        }
    };
}

TEST_F(StepMetricsTest, first_order_response_has_textbook_rise_and_settling_times)
{
    const float tau = 0.010f;
    const auto normalised = NormaliseStep(FirstOrder(20.0f, tau, speedWindowSamples), 0.0f, 20.0f);

    const auto metrics = ComputeStepMetrics<speedWindowSamples>(normalised, dt, 0.02f, 20.0f);

    ASSERT_TRUE(metrics.has_value());
    EXPECT_NEAR(metrics->riseTimeS, 2.2f * tau, 0.002f);
    EXPECT_NEAR(metrics->settlingTimeS, 3.9f * tau, 0.002f);
    EXPECT_LE(metrics->percentOvershoot, 0.0f);
    EXPECT_NEAR(metrics->steadyStateError, 0.0f, 0.01f);
}

TEST_F(StepMetricsTest, underdamped_response_reports_the_analytic_overshoot)
{
    const float zeta = 0.5f;
    const auto normalised = NormaliseStep(SecondOrder(1.5f, 100.0f, zeta, positionWindowSamples), 0.0f, 1.5f);

    const auto metrics = ComputeStepMetrics<positionWindowSamples>(normalised, dt, 0.02f, 1.5f);

    ASSERT_TRUE(metrics.has_value());
    const float expected = 100.0f * std::exp(-zeta * std::numbers::pi_v<float> / std::sqrt(1.0f - zeta * zeta));
    EXPECT_NEAR(metrics->percentOvershoot, expected, 1.0f);
    EXPECT_NEAR(metrics->peakTimeS, std::numbers::pi_v<float> / (100.0f * std::sqrt(1.0f - zeta * zeta)), 0.002f);
}

TEST_F(StepMetricsTest, a_negative_step_normalises_to_a_unit_step)
{
    std::vector<float> raw;
    for (const float y : FirstOrder(40.0f, 0.010f, speedWindowSamples))
        raw.push_back(20.0f - y);

    const auto normalised = NormaliseStep(raw, 20.0f, -20.0f);
    const auto metrics = ComputeStepMetrics<speedWindowSamples>(normalised, dt, 0.02f, -40.0f);

    ASSERT_TRUE(metrics.has_value());
    EXPECT_NEAR(normalised.back(), 1.0f, 0.01f);
    EXPECT_LE(metrics->percentOvershoot, 0.0f);
    EXPECT_NEAR(metrics->steadyStateError, 0.0f, 0.05f);
}

TEST_F(StepMetricsTest, steady_state_error_is_scaled_back_to_signal_units)
{
    std::vector<float> raw(speedWindowSamples, 19.0f);

    const auto metrics = ComputeStepMetrics<speedWindowSamples>(NormaliseStep(raw, 0.0f, 20.0f), dt, 0.02f, 20.0f);

    ASSERT_TRUE(metrics.has_value());
    EXPECT_NEAR(metrics->steadyStateError, 1.0f, 1e-3f);
}

TEST_F(StepMetricsTest, too_few_samples_yields_no_metrics)
{
    const std::vector<float> raw(speedWindowSamples - 1, 1.0f);

    EXPECT_FALSE(ComputeStepMetrics<speedWindowSamples>(raw, dt, 0.02f, 1.0f).has_value());
}

TEST_F(StepMetricsTest, disturbance_metrics_report_dip_and_recovery)
{
    std::vector<float> raw;
    for (std::size_t i = 0; i != disturbanceWindowSamples; ++i)
    {
        const float t = static_cast<float>(i) * dt;
        raw.push_back(20.0f - 5.0f * (t / 0.010f) * std::exp(1.0f - t / 0.010f));
    }

    const auto metrics = ComputeDisturbanceMetrics<disturbanceWindowSamples>(raw, 20.0f, dt, 0.4f);

    ASSERT_TRUE(metrics.has_value());
    EXPECT_NEAR(metrics->maxDeviation, 5.0f, 0.1f);
    EXPECT_GT(metrics->recoveryTimeS, 0.010f);
    EXPECT_LT(metrics->recoveryTimeS, 0.100f);
    EXPECT_NEAR(metrics->steadyStateError, 0.0f, 0.05f);
}

TEST_F(StepMetricsTest, disturbance_that_never_leaves_the_band_recovers_immediately)
{
    const std::vector<float> raw(disturbanceWindowSamples, 20.1f);

    const auto metrics = ComputeDisturbanceMetrics<disturbanceWindowSamples>(raw, 20.0f, dt, 0.4f);

    ASSERT_TRUE(metrics.has_value());
    EXPECT_NEAR(metrics->recoveryTimeS, 0.0f, 1e-6f);
    EXPECT_NEAR(metrics->maxDeviation, 0.1f, 1e-4f);
}

TEST_F(StepMetricsTest, dispatch_by_signal_uses_the_window_of_that_loop)
{
    const auto normalised = NormaliseStep(FirstOrder(0.5f, 0.001f, currentWindowSamples), 0.0f, 0.5f);

    EXPECT_TRUE(ComputeStepMetricsFor(Signal::currentQ, normalised, 0.00005f, 0.02f, 0.5f).has_value());
    EXPECT_FALSE(ComputeStepMetricsFor(Signal::speed, normalised, dt, 0.02f, 0.5f).has_value());
}
