#include "core/foc/cascade/MechanicalWindowAccumulator.hpp"
#include "core/services/mechanical_system_ident/MechanicalWindowKinematics.hpp"
#include <array>
#include <cmath>
#include <functional>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float samplePeriod{ 1.0f / 20000.0f };
    constexpr uint32_t samplesPerWindow{ 20 };
    constexpr uint32_t windowsPerObservation{ 4 };
    constexpr uint32_t samplesPerObservation{ samplesPerWindow * windowsPerObservation };
    constexpr float observationPeriod{ samplePeriod * samplesPerObservation };

    float CountOf(uint32_t countsPerRevolution)
    {
        return 2.0f * std::numbers::pi_v<float> / static_cast<float>(countsPerRevolution);
    }

    // Samples a trajectory sample by sample into outer-loop windows, the way the control interrupt does.
    class TrajectoryWindows
    {
    public:
        TrajectoryWindows(std::function<float(float)> position, std::function<float(float)> current)
            : position{ std::move(position) }
            , current{ std::move(current) }
        {}

        foc::MechanicalWindow Next()
        {
            for (uint32_t sample = 0; sample != samplesPerWindow; ++sample)
            {
                const auto time = static_cast<float>(index++) * samplePeriod;
                const auto angle = position(time);
                accumulator.Accumulate(current(time), primed ? angle - previousAngle : 0.0f);
                previousAngle = angle;
                primed = true;
            }

            return accumulator.Close();
        }

    private:
        foc::MechanicalWindowAccumulator accumulator{ samplesPerWindow };
        std::function<float(float)> position;
        std::function<float(float)> current;
        uint32_t index{ 0 };
        float previousAngle{ 0.0f };
        bool primed{ false };
    };

    // The observation computed directly from the samples of the three observation windows that start at the given
    // sample: window-mean second difference and the current weighted by the quadratic B-spline, sample by sample.
    services::MechanicalObservation Expected(const std::function<float(float)>& position, const std::function<float(float)>& current, uint32_t firstSample)
    {
        std::array<float, 3> means{};
        float iq{ 0.0f };

        for (uint32_t sample = 0; sample != 3 * samplesPerObservation; ++sample)
        {
            const auto time = static_cast<float>(firstSample + sample) * samplePeriod;
            const auto window = sample / samplesPerObservation;
            const auto u = (static_cast<float>(sample % samplesPerObservation) + 0.5f) / static_cast<float>(samplesPerObservation);
            const auto weight = window == 0 ? 0.5f * u * u : (window == 1 ? 0.5f + u * (1.0f - u) : 0.5f * (1.0f - u) * (1.0f - u));

            means.at(window) += position(time) / static_cast<float>(samplesPerObservation);
            iq += weight * current(time) / static_cast<float>(samplesPerObservation);
        }

        return { (means[2] - 2.0f * means[1] + means[0]) / (observationPeriod * observationPeriod), (means[2] - means[0]) / (2.0f * observationPeriod), iq };
    }

    std::optional<services::MechanicalObservation> Feed(services::MechanicalWindowKinematics& kinematics, TrajectoryWindows& windows, uint32_t count)
    {
        std::optional<services::MechanicalObservation> last;
        for (uint32_t window = 0; window != count; ++window)
            if (const auto observation = kinematics.Add(windows.Next()))
                last = observation;

        return last;
    }

    class TestMechanicalWindowKinematics
        : public ::testing::Test
    {
    public:
        services::MechanicalWindowKinematics kinematics{ samplePeriod * samplesPerWindow };
    };
}

TEST_F(TestMechanicalWindowKinematics, a_constant_acceleration_is_measured_from_window_mean_positions)
{
    constexpr float acceleration{ 500.0f };
    TrajectoryWindows windows{ [](float t)
        {
            return 30.0f * t + 0.5f * acceleration * t * t;
        },
        [](float)
        {
            return 1.5f;
        } };

    const auto observation = Feed(kinematics, windows, 3 * windowsPerObservation);

    ASSERT_TRUE(observation.has_value());
    EXPECT_NEAR(observation->acceleration, acceleration, 0.01f * acceleration);
    EXPECT_NEAR(observation->iq, 1.5f, 1e-4f);
}

TEST_F(TestMechanicalWindowKinematics, merged_windows_observe_the_kernel_of_their_samples)
{
    const auto position = [](float t)
    {
        return 30.0f * t + 0.5f * 800.0f * t * t + 0.002f * std::sin(2.0f * std::numbers::pi_v<float> * 40.0f * t);
    };
    const auto current = [](float t)
    {
        return 1.0f + std::sin(2.0f * std::numbers::pi_v<float> * 60.0f * t) + 400.0f * t;
    };
    TrajectoryWindows windows{ position, current };

    const auto observation = Feed(kinematics, windows, 3 * windowsPerObservation);
    const auto expected = Expected(position, current, 0);

    ASSERT_TRUE(observation.has_value());
    EXPECT_NEAR(observation->acceleration, expected.acceleration, 1e-3f * std::abs(expected.acceleration));
    EXPECT_NEAR(observation->speed, expected.speed, 1e-4f * std::abs(expected.speed));
    EXPECT_NEAR(observation->iq, expected.iq, 1e-4f);
}

TEST_F(TestMechanicalWindowKinematics, the_observation_window_follows_the_encoder_count)
{
    const auto windowsFor = [](float count)
    {
        services::MechanicalWindowKinematics sized{ samplePeriod * samplesPerWindow };
        foc::MechanicalWindow window;
        window.smallestStep = foc::Radians{ count };
        for (int i = 0; i != 8; ++i)
        {
            sized.Add(window);
            window.first = false;
        }
        return sized.WindowsPerObservation();
    };

    EXPECT_EQ(windowsFor(1e-7f), 4u);
    EXPECT_EQ(windowsFor(CountOf(4000)), 4u);
    EXPECT_EQ(windowsFor(CountOf(1000)), 8u);
    EXPECT_EQ(windowsFor(CountOf(100)), 16u);
}

TEST_F(TestMechanicalWindowKinematics, a_first_window_starts_the_run_of_three_again)
{
    TrajectoryWindows windows{ [](float t)
        {
            return 30.0f * t;
        },
        [](float)
        {
            return 1.0f;
        } };
    ASSERT_TRUE(Feed(kinematics, windows, 4 * windowsPerObservation).has_value());

    auto restarted = windows.Next();
    restarted.first = true;
    EXPECT_FALSE(kinematics.Add(restarted).has_value());

    EXPECT_FALSE(Feed(kinematics, windows, 3 * windowsPerObservation - 2).has_value());
    EXPECT_TRUE(kinematics.Add(windows.Next()).has_value());
}
