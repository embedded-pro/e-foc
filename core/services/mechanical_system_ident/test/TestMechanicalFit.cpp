#include "core/services/mechanical_system_ident/MechanicalFit.hpp"
#include "gtest/gtest.h"
#include <cmath>

namespace
{
    using services::IdentificationObservation;
    using services::MechanicalFit;

    constexpr float coulomb{ 0.01f };
    constexpr float inertia{ 1.0e-4f };
    constexpr float viscous{ 1.0e-3f };

    IdentificationObservation Observation(uint32_t sample)
    {
        const auto acceleration = 100.0f * std::sin(0.37f * static_cast<float>(sample));
        const auto speed = 5.0f + 3.0f * std::cos(0.11f * static_cast<float>(sample));
        return IdentificationObservation{ acceleration, speed, coulomb + inertia * acceleration + viscous * speed, sample, 1 };
    }

    class TestMechanicalFit
        : public testing::Test
    {
    public:
        void Feed(uint32_t first, uint32_t last)
        {
            for (auto sample = first; sample != last; ++sample)
                fit.Fit(Observation(sample));
        }

        MechanicalFit fit;
    };
}

TEST_F(TestMechanicalFit, a_restarted_fit_runs_until_stopped)
{
    EXPECT_FALSE(fit.IsRunning());

    fit.Restart(0.99f, 100, 200);
    EXPECT_TRUE(fit.IsRunning());
    EXPECT_FALSE(fit.HasConverged());
    EXPECT_FALSE(fit.IsFinishedAt(1000000));

    fit.Stop();
    EXPECT_FALSE(fit.IsRunning());
}

TEST_F(TestMechanicalFit, consistent_observations_yield_the_inertia_and_viscous_friction)
{
    fit.Restart(0.99f, 100, 200);
    Feed(1, 1000);

    const auto estimate = fit.Estimate();
    EXPECT_NEAR(inertia, estimate.inertia, inertia * 0.01f);
    EXPECT_NEAR(viscous, estimate.friction, viscous * 0.01f);
}

TEST_F(TestMechanicalFit, convergence_finishes_the_run_at_the_end_of_the_current_dwell)
{
    fit.Restart(0.99f, 100, 200);
    uint32_t sample = 1;
    while (!fit.HasConverged() && sample != 5000)
        fit.Fit(Observation(sample++));

    ASSERT_TRUE(fit.HasConverged());
    const auto convergedAt = sample - 1;
    EXPECT_GE(convergedAt, 200u);

    const auto finishAt = (convergedAt / 100 + 1) * 100;
    EXPECT_FALSE(fit.IsFinishedAt(finishAt - 1));
    EXPECT_TRUE(fit.IsFinishedAt(finishAt));
}

TEST_F(TestMechanicalFit, observations_at_or_after_the_finish_are_ignored)
{
    fit.Restart(0.99f, 100, 200);
    uint32_t sample = 1;
    while (!fit.HasConverged() && sample != 5000)
        fit.Fit(Observation(sample++));
    ASSERT_TRUE(fit.HasConverged());
    const auto before = fit.Estimate();

    for (uint32_t late = 0; late != 50; ++late)
        fit.Fit(IdentificationObservation{ 100.0f, 5.0f, 1.0f, 100000 + late, 1 });

    EXPECT_NEAR(before.inertia, fit.Estimate().inertia, 1e-9f);
}
