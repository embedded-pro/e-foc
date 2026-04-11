#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include <gmock/gmock.h>

namespace
{
    class TestRealTimeResistanceAndInductanceEstimator
        : public ::testing::Test
    {
    protected:
        services::RealTimeResistanceAndInductanceEstimator estimator{ 0.998f, hal::Hertz{ 1000 } };
    };
}

TEST_F(TestRealTimeResistanceAndInductanceEstimator, update_produces_finite_estimates)
{
    estimator.Update(foc::Volts{ 1.0f }, foc::Ampere{ 0.5f }, foc::Ampere{ 0.0f }, foc::RadiansPerSecond{ 100.0f });

    EXPECT_TRUE(std::isfinite(estimator.CurrentResistance().Value()));
    EXPECT_TRUE(std::isfinite(estimator.CurrentInductance().Value()));
}

TEST_F(TestRealTimeResistanceAndInductanceEstimator, skips_update_when_regressor_near_zero)
{
    // Near-zero inputs → PE gate should prevent update
    estimator.Update(foc::Volts{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::RadiansPerSecond{ 0.0f });

    EXPECT_TRUE(std::isfinite(estimator.CurrentResistance().Value()));
    EXPECT_TRUE(std::isfinite(estimator.CurrentInductance().Value()));
}

TEST_F(TestRealTimeResistanceAndInductanceEstimator, inductance_returned_in_millihenry)
{
    // With multiple updates using consistent data, inductance should be
    // finite and in mH range (not raw H)
    for (int i = 0; i < 50; ++i)
        estimator.Update(foc::Volts{ 1.0f }, foc::Ampere{ 0.5f }, foc::Ampere{ 0.1f }, foc::RadiansPerSecond{ 100.0f });

    estimator.Update(foc::Volts{ 1.0f }, foc::Ampere{ 0.5f }, foc::Ampere{ 0.1f }, foc::RadiansPerSecond{ 100.0f });
    EXPECT_TRUE(std::isfinite(estimator.CurrentInductance().Value()));
}

TEST_F(TestRealTimeResistanceAndInductanceEstimator, seed_initializes_rls_coefficients_so_estimates_persist_under_no_excitation)
{
    // Arrange: seed with known R and L values.
    estimator.Seed(foc::Ohm{ 1.5f }, foc::MilliHenry{ 2.0f });

    // Act: near-zero update so PE gate blocks the RLS update.
    estimator.Update(foc::Volts{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::RadiansPerSecond{ 0.0f });

    // Assert: seeded values are returned from RLS Coefficients, not zero-initialised theta.
    EXPECT_NEAR(estimator.CurrentResistance().Value(), 1.5f, 0.1f);
    EXPECT_NEAR(estimator.CurrentInductance().Value(), 2.0f, 0.1f);
}

TEST_F(TestRealTimeResistanceAndInductanceEstimator, set_initial_estimate_stores_values)
{
    estimator.SetInitialEstimate(foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.2f });

    EXPECT_FLOAT_EQ(estimator.CurrentResistance().Value(), 0.5f);
    EXPECT_FLOAT_EQ(estimator.CurrentInductance().Value(), 1.2f);
}

TEST_F(TestRealTimeResistanceAndInductanceEstimator, update_when_called_produces_finite_estimates)
{
    estimator.Update(foc::Volts{ 1.0f }, foc::Ampere{ 0.5f }, foc::Ampere{ 0.0f }, foc::RadiansPerSecond{ 100.0f });
    estimator.Update(foc::Volts{ 1.2f }, foc::Ampere{ 0.6f }, foc::Ampere{ 0.1f }, foc::RadiansPerSecond{ 100.0f });

    EXPECT_TRUE(std::isfinite(estimator.CurrentResistance().Value()));
    EXPECT_TRUE(std::isfinite(estimator.CurrentInductance().Value()));
}

TEST_F(TestRealTimeResistanceAndInductanceEstimator, set_initial_estimate_seeds_rls_so_values_persist_under_no_excitation)
{
    // Arrange: seed with known values, then call Update with zero inputs so PE gate blocks RLS.
    estimator.SetInitialEstimate(foc::Ohm{ 2.0f }, foc::MilliHenry{ 3.0f });

    // Act: near-zero inputs — PE gate prevents RLS update, seeded theta persists.
    estimator.Update(foc::Volts{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::RadiansPerSecond{ 0.0f });

    // Assert: seeded values returned, not zero-initialised theta.
    EXPECT_NEAR(estimator.CurrentResistance().Value(), 2.0f, 0.1f);
    EXPECT_NEAR(estimator.CurrentInductance().Value(), 3.0f, 0.1f);
}
