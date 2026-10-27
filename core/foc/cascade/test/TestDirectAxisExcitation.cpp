#include "core/foc/cascade/DirectAxisExcitation.hpp"
#include <cmath>
#include <gtest/gtest.h>

namespace
{
    constexpr float amplitude{ 0.075f };
    const hal::Hertz outerLoopFrequency{ 100 };
    constexpr uint32_t burstSamples{ 100 };
    constexpr uint32_t cycleSamples{ 300 };

    class TestDirectAxisExcitation
        : public ::testing::Test
    {
    public:
        void Skip(uint32_t samples)
        {
            for (uint32_t i = 0; i != samples; ++i)
                excitation.Advance();
        }

        foc::DirectAxisExcitation excitation{ foc::Ampere{ amplitude }, hal::Hertz{ 10 }, outerLoopFrequency, std::chrono::seconds{ 1 }, std::chrono::seconds{ 3 } };
    };
}

TEST_F(TestDirectAxisExcitation, the_burst_alternates_between_the_two_signs_of_the_amplitude)
{
    uint32_t positives{ 0 };
    uint32_t negatives{ 0 };
    for (uint32_t i = 0; i != burstSamples - 1; ++i)
    {
        const auto value = excitation.Advance();
        EXPECT_NEAR(std::abs(value), amplitude, 1e-6f) << " sample " << i;
        (value > 0.0f ? positives : negatives) += 1;
    }

    EXPECT_NEAR(static_cast<float>(positives), static_cast<float>(negatives), 5.0f);
}

TEST_F(TestDirectAxisExcitation, the_reference_is_zero_between_bursts)
{
    Skip(burstSamples - 1);

    for (uint32_t i = burstSamples; i != cycleSamples; ++i)
        EXPECT_NEAR(excitation.Advance(), 0.0f, 1e-6f) << " sample " << i;
}

TEST_F(TestDirectAxisExcitation, a_new_burst_starts_every_cycle_on_the_positive_step)
{
    Skip(cycleSamples - 1);

    EXPECT_NEAR(excitation.Advance(), amplitude, 1e-6f);
}

TEST_F(TestDirectAxisExcitation, restarting_starts_a_burst_at_once)
{
    Skip(burstSamples + 10);

    excitation.Restart();

    EXPECT_NEAR(excitation.Advance(), amplitude, 1e-6f);
}

TEST_F(TestDirectAxisExcitation, a_zero_frequency_keeps_the_reference_at_zero)
{
    foc::DirectAxisExcitation off{ foc::Ampere{ amplitude }, hal::Hertz{ 0 }, outerLoopFrequency, std::chrono::seconds{ 1 }, std::chrono::seconds{ 3 } };

    for (uint32_t i = 0; i != cycleSamples; ++i)
        EXPECT_NEAR(off.Advance(), 0.0f, 1e-6f);
}
