#include "core/services/electrical_system_ident/RealTimeFluxLinkageEstimator.hpp"
#include <gtest/gtest.h>

namespace
{
    constexpr float outerFrequency{ 1000.0f };
    constexpr float resistance{ 0.4f };
    constexpr float inductance{ 0.2f };
    constexpr float seed{ 0.01f };

    struct Operating
    {
        float fluxLinkage{ 0.01f };
        float speed{ 400.0f };
        float iq{ 2.0f };
        float id{ 0.1f };
        float iqChange{ 0.0f };
    };

    class TestRealTimeFluxLinkageEstimator
        : public ::testing::Test
    {
    protected:
        TestRealTimeFluxLinkageEstimator()
        {
            estimator.SetInitialEstimate(foc::Weber{ seed });
        }

        foc::QuadratureWindow WindowFor(const Operating& operating, bool first = false)
        {
            iqAtEnd += operating.iqChange;
            const auto voltage = resistance * operating.iq + inductance * 1e-3f * operating.iqChange * outerFrequency + operating.speed * (inductance * 1e-3f * operating.id + operating.fluxLinkage);
            return foc::QuadratureWindow{ foc::Volts{ voltage }, foc::Ampere{ operating.iq }, foc::Ampere{ iqAtEnd }, foc::Ampere{ operating.id },
                foc::RadiansPerSecond{ operating.speed }, foc::Ohm{ resistance }, foc::MilliHenry{ inductance }, foc::Volts{ 14.0f }, first };
        }

        void Run(const Operating& operating, int windows)
        {
            for (int window = 0; window != windows; ++window)
                estimator.Update(WindowFor(operating));
        }

        services::RealTimeFluxLinkageEstimator estimator{ services::RealTimeFluxLinkageEstimator::defaultForgettingFactor, hal::Hertz{ 1000 } };
        float iqAtEnd{ 0.0f };
    };
}

TEST_F(TestRealTimeFluxLinkageEstimator, the_seed_is_reported_before_the_estimator_has_settled)
{
    Operating hot;
    hot.fluxLinkage = 0.009f;

    Run(hot, services::RealTimeFluxLinkageEstimator::minimumUpdates - 1);

    EXPECT_NEAR(estimator.CurrentFluxLinkage().Value(), seed, 1e-9f);
    EXPECT_FALSE(estimator.HasSettled());
}

TEST_F(TestRealTimeFluxLinkageEstimator, a_weaker_magnet_is_identified_after_the_minimum_updates)
{
    Operating hot;
    hot.fluxLinkage = 0.009f;

    Run(hot, services::RealTimeFluxLinkageEstimator::minimumUpdates);

    EXPECT_TRUE(estimator.HasSettled());
    EXPECT_NEAR(estimator.CurrentFluxLinkage().Value(), 0.009f, 1e-5f);
}

TEST_F(TestRealTimeFluxLinkageEstimator, the_resistive_drop_is_not_taken_for_back_emf)
{
    Operating loaded;
    loaded.iq = 6.0f;

    Run(loaded, 300);

    EXPECT_NEAR(estimator.CurrentFluxLinkage().Value(), seed, 1e-5f);
}

TEST_F(TestRealTimeFluxLinkageEstimator, the_inductive_drop_of_a_changing_current_is_not_taken_for_back_emf)
{
    Operating accelerating;
    accelerating.iqChange = 0.5f;

    Run(accelerating, 300);

    EXPECT_NEAR(estimator.CurrentFluxLinkage().Value(), seed, 1e-5f);
}

TEST_F(TestRealTimeFluxLinkageEstimator, the_direct_axis_cross_coupling_is_not_taken_for_back_emf)
{
    Operating fieldWeakened;
    fieldWeakened.id = -3.0f;

    Run(fieldWeakened, 300);

    EXPECT_NEAR(estimator.CurrentFluxLinkage().Value(), seed, 1e-5f);
}

TEST_F(TestRealTimeFluxLinkageEstimator, a_negative_speed_gives_the_same_flux_linkage)
{
    Operating reverse;
    reverse.fluxLinkage = 0.009f;
    reverse.speed = -400.0f;
    reverse.iq = -2.0f;

    Run(reverse, 300);

    EXPECT_NEAR(estimator.CurrentFluxLinkage().Value(), 0.009f, 1e-5f);
}

TEST_F(TestRealTimeFluxLinkageEstimator, windows_too_slow_to_carry_back_emf_are_skipped)
{
    Operating crawling;
    crawling.fluxLinkage = 0.005f;
    crawling.speed = 10.0f;

    Run(crawling, 400);

    EXPECT_NEAR(estimator.CurrentFluxLinkage().Value(), seed, 1e-9f);
    EXPECT_FALSE(estimator.HasSettled());
}

TEST_F(TestRealTimeFluxLinkageEstimator, the_first_window_after_a_restart_is_skipped)
{
    Operating hot;
    hot.fluxLinkage = 0.009f;

    for (int window = 0; window != 400; ++window)
        estimator.Update(WindowFor(hot, true));

    EXPECT_FALSE(estimator.HasSettled());
    EXPECT_NEAR(estimator.CurrentFluxLinkage().Value(), seed, 1e-9f);
}

TEST_F(TestRealTimeFluxLinkageEstimator, a_reading_that_is_not_positive_is_not_published)
{
    Operating broken;
    broken.fluxLinkage = -0.004f;

    Run(broken, 400);

    EXPECT_FALSE(estimator.HasSettled());
    EXPECT_NEAR(estimator.CurrentFluxLinkage().Value(), seed, 1e-9f);
}

TEST_F(TestRealTimeFluxLinkageEstimator, a_new_seed_discards_what_was_learned)
{
    Operating hot;
    hot.fluxLinkage = 0.009f;
    Run(hot, 400);
    ASSERT_TRUE(estimator.HasSettled());

    estimator.SetInitialEstimate(foc::Weber{ 0.02f });

    EXPECT_FALSE(estimator.HasSettled());
    EXPECT_NEAR(estimator.CurrentFluxLinkage().Value(), 0.02f, 1e-9f);
}

TEST_F(TestRealTimeFluxLinkageEstimator, a_drifting_magnet_is_followed)
{
    Operating warming;
    warming.fluxLinkage = 0.0099f;
    Run(warming, 400);

    warming.fluxLinkage = 0.0092f;
    Run(warming, 4000);

    EXPECT_NEAR(estimator.CurrentFluxLinkage().Value(), 0.0092f, 5e-5f);
}
