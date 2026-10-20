#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <random>

namespace
{
    struct Winding
    {
        float resistance{ 0.36f };
        float inductance{ 0.2e-3f };
        float electricalSpeed{ 208.0f };
        float quadratureCurrent{ 0.05f };
        float currentNoise{ 0.0f };
    };

    // A winding under a d-axis current loop, driven by the square-wave reference and averaged over each outer-loop
    // window as the control interrupt does. Every current sample, the one the loop regulates on included, can carry
    // white sensor noise.

    class WindowedWindingRun
    {
    public:
        WindowedWindingRun(const Winding& winding, services::RealTimeResistanceAndInductanceEstimator& estimator)
            : winding{ winding }
            , estimator{ estimator }
        {}

        void Run(float amplitude, int windows)
        {
            constexpr int ticksPerWindow{ 20 };
            constexpr int windowsPerHalfPeriod{ 50 };
            constexpr int substeps{ 10 };
            constexpr float tick{ 1.0f / 20000.0f };
            const float gain{ winding.inductance * 2000.0f };

            for (int window = 0; window != windows; ++window)
            {
                const float reference = (window / windowsPerHalfPeriod) % 2 == 0 ? amplitude : -amplitude;
                float voltageSum{ 0.0f };
                float currentSum{ 0.0f };

                for (int t = 0; t != ticksPerWindow; ++t)
                {
                    const float applied = pendingVoltage;
                    const float previous = measured;

                    for (int substep = 0; substep != substeps; ++substep)
                        current += (applied - winding.resistance * current + winding.electricalSpeed * winding.inductance * winding.quadratureCurrent) / winding.inductance * tick / substeps;

                    measured = current + winding.currentNoise * noise(engine);
                    voltageSum += applied;
                    currentSum += 0.5f * (measured + previous);
                    pendingVoltage = gain * (reference - measured) + winding.resistance * reference - winding.electricalSpeed * winding.inductance * winding.quadratureCurrent;
                }

                estimator.Update(foc::ElectricalWindow{ foc::Volts{ voltageSum / ticksPerWindow }, foc::Ampere{ currentSum / ticksPerWindow },
                    foc::Ampere{ measured }, winding.electricalSpeed * winding.quadratureCurrent, foc::Ampere{ reference } });
            }
        }

    private:
        Winding winding;
        services::RealTimeResistanceAndInductanceEstimator& estimator;
        float pendingVoltage{ 0.0f };
        float current{ 0.0f };
        float measured{ 0.0f };
        std::mt19937 engine{ 3 };
        std::normal_distribution<float> noise{ 0.0f, 1.0f };
    };

    class TestRealTimeResistanceAndInductanceEstimator
        : public ::testing::Test
    {
    protected:
        services::RealTimeResistanceAndInductanceEstimator estimator{ services::RealTimeResistanceAndInductanceEstimator::defaultForgettingFactor, hal::Hertz{ 1000 } };
    };
}

TEST_F(TestRealTimeResistanceAndInductanceEstimator, set_initial_estimate_stores_values)
{
    estimator.SetInitialEstimate(foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.2f });

    EXPECT_FLOAT_EQ(estimator.CurrentResistance().Value(), 0.5f);
    EXPECT_FLOAT_EQ(estimator.CurrentInductance().Value(), 1.2f);
}

TEST_F(TestRealTimeResistanceAndInductanceEstimator, without_a_direct_axis_current_the_seed_is_kept)
{
    const Winding winding;
    estimator.SetInitialEstimate(foc::Ohm{ 2.0f }, foc::MilliHenry{ 3.0f });

    WindowedWindingRun{ winding, estimator }.Run(0.0f, 2000);

    EXPECT_FLOAT_EQ(estimator.CurrentResistance().Value(), 2.0f);
    EXPECT_FLOAT_EQ(estimator.CurrentInductance().Value(), 3.0f);
}

TEST_F(TestRealTimeResistanceAndInductanceEstimator, too_few_updates_are_not_published)
{
    const Winding winding;
    estimator.SetInitialEstimate(foc::Ohm{ 2.0f }, foc::MilliHenry{ 3.0f });

    WindowedWindingRun{ winding, estimator }.Run(0.5f, 100);

    EXPECT_FLOAT_EQ(estimator.CurrentResistance().Value(), 2.0f);
    EXPECT_FLOAT_EQ(estimator.CurrentInductance().Value(), 3.0f);
}

TEST_F(TestRealTimeResistanceAndInductanceEstimator, a_direct_axis_square_wave_identifies_the_winding_from_a_wrong_seed)
{
    const Winding winding;
    estimator.SetInitialEstimate(foc::Ohm{ 1.3f * winding.resistance }, foc::MilliHenry{ 0.7f * winding.inductance * 1000.0f });

    WindowedWindingRun{ winding, estimator }.Run(0.5f, 3000);

    EXPECT_NEAR(estimator.CurrentResistance().Value(), winding.resistance, 0.03f * winding.resistance);
    EXPECT_NEAR(estimator.CurrentInductance().Value(), winding.inductance * 1000.0f, 0.05f * winding.inductance * 1000.0f);
}

TEST_F(TestRealTimeResistanceAndInductanceEstimator, a_warmer_winding_is_tracked)
{
    Winding winding;
    estimator.SetInitialEstimate(foc::Ohm{ winding.resistance }, foc::MilliHenry{ winding.inductance * 1000.0f });
    winding.resistance *= 1.25f;

    WindowedWindingRun{ winding, estimator }.Run(0.5f, 3000);

    EXPECT_NEAR(estimator.CurrentResistance().Value(), winding.resistance, 0.03f * winding.resistance);
}

TEST_F(TestRealTimeResistanceAndInductanceEstimator, sensor_noise_on_the_current_samples_leaves_the_inductance_unbiased)
{
    Winding winding;
    winding.currentNoise = 0.05f;
    estimator.SetInitialEstimate(foc::Ohm{ 1.3f * winding.resistance }, foc::MilliHenry{ 0.7f * winding.inductance * 1000.0f });

    WindowedWindingRun{ winding, estimator }.Run(0.5f, 6000);

    EXPECT_NEAR(estimator.CurrentResistance().Value(), winding.resistance, 0.03f * winding.resistance);
    EXPECT_NEAR(estimator.CurrentInductance().Value(), winding.inductance * 1000.0f, 0.05f * winding.inductance * 1000.0f);
}
