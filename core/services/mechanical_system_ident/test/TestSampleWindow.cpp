#include "core/services/mechanical_system_ident/SampleWindow.hpp"
#include "gtest/gtest.h"
#include <vector>

namespace
{
    using services::ClosedWindow;
    using services::SampleWindow;

    constexpr float samplingPeriod{ 1.0e-3f };

    class TestSampleWindow
        : public testing::Test
    {
    public:
        std::vector<ClosedWindow> Run(float speed, float torque, std::size_t samples)
        {
            std::vector<ClosedWindow> closed;
            for (std::size_t sample = 0; sample != samples; ++sample)
            {
                position += speed * samplingPeriod;
                if (const auto window = sampleWindow.Add(position, torque); window.has_value())
                    closed.push_back(*window);
            }
            return closed;
        }

        SampleWindow sampleWindow;
        float position{ 0.0f };
    };
}

TEST_F(TestSampleWindow, a_fine_encoder_closes_a_window_every_sample)
{
    sampleWindow.Restart(position, samplingPeriod);

    const auto closed = Run(0.05f, 0.2f, 12);

    ASSERT_EQ(12u, closed.size());
    EXPECT_FALSE(closed[0].hadMean);
    EXPECT_FALSE(closed[0].hadSpeed);
    EXPECT_TRUE(closed[1].hadMean);
    EXPECT_FALSE(closed[1].hadSpeed);
    EXPECT_TRUE(closed[2].hadSpeed);
    EXPECT_FALSE(closed[6].resolutionKnown);
    EXPECT_TRUE(closed[7].resolutionKnown);

    for (const auto& window : closed)
    {
        EXPECT_NEAR(0.05f, window.speed, 1e-3f);
        EXPECT_NEAR(0.2f, window.centredTorque, 1e-6f);
        EXPECT_EQ(1u, window.windowSamples);
    }

    EXPECT_NEAR(0.0f, closed[11].acceleration, 0.5f);
    EXPECT_EQ(12u, closed[11].sample);
}

TEST_F(TestSampleWindow, once_the_resolution_is_known_the_window_grows_and_its_pairing_restarts)
{
    sampleWindow.Restart(position, samplingPeriod);

    const auto closed = Run(10.0f, 0.3f, 38);

    ASSERT_EQ(10u, closed.size());
    EXPECT_EQ(7u, closed[6].sample);

    const auto& firstLong = closed[7];
    EXPECT_EQ(18u, firstLong.sample);
    EXPECT_EQ(10u, firstLong.windowSamples);
    EXPECT_FALSE(firstLong.hadMean);

    const auto& secondLong = closed[8];
    EXPECT_TRUE(secondLong.hadMean);
    EXPECT_NEAR(10.0f, secondLong.speed, 1e-3f);

    const auto& thirdLong = closed[9];
    EXPECT_EQ(38u, thirdLong.sample);
    EXPECT_TRUE(thirdLong.hadSpeed);
    EXPECT_NEAR(10.0f, thirdLong.centredSpeed, 1e-3f);
    EXPECT_NEAR(0.3f, thirdLong.centredTorque, 1e-5f);
    EXPECT_NEAR(0.0f, thirdLong.acceleration, 1e-2f);
}

TEST_F(TestSampleWindow, a_torque_ramp_is_centred_on_the_middle_window)
{
    sampleWindow.Restart(position, samplingPeriod);
    Run(10.0f, 0.0f, 8);

    std::vector<ClosedWindow> closed;
    float torque{ 0.0f };
    for (std::size_t sample = 0; sample != 40; ++sample)
    {
        position += 10.0f * samplingPeriod;
        torque += 0.01f;
        if (const auto window = sampleWindow.Add(position, torque); window.has_value())
            closed.push_back(*window);
    }

    ASSERT_EQ(4u, closed.size());
    const auto middleOfPreviousWindow = 0.01f * (static_cast<float>(closed[3].sample - 8) - 1.5f * 10.0f + 0.5f);
    EXPECT_NEAR(middleOfPreviousWindow, closed[3].centredTorque, 1e-4f);
}
