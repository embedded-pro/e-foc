#include "core/services/current_offsets/ConfirmedPhaseCurrentOffsets.hpp"
#include "core/services/current_offsets/test_doubles/PhaseCurrentOffsetsMock.hpp"
#include <gmock/gmock.h>
#include <optional>

namespace
{
    using namespace testing;

    foc::PhaseCurrents Currents(float a, float b, float c)
    {
        return { foc::Ampere{ a }, foc::Ampere{ b }, foc::Ampere{ c } };
    }

    class TestConfirmedPhaseCurrentOffsets
        : public ::testing::Test
    {
    public:
        void StartMeasurement()
        {
            ExpectWindow();
            confirmed.Measure([this](std::optional<foc::PhaseCurrents> offsets)
                {
                    measured = offsets;
                    completed = true;
                });
        }

        void ExpectWindow()
        {
            EXPECT_CALL(measurementMock, Measure(_)).WillOnce([this](const auto& onDone)
                {
                    onWindow = onDone;
                });
        }

        void CompleteWindow(std::optional<foc::PhaseCurrents> offsets, float noiseAmpere, bool another)
        {
            if (offsets.has_value())
                EXPECT_CALL(measurementMock, Noise()).WillOnce(Return(foc::Ampere{ noiseAmpere }));
            if (another)
                ExpectWindow();

            auto window = onWindow;
            onWindow = nullptr;
            window(offsets);
        }

        void ExpectOffsets(const foc::PhaseCurrents& expected)
        {
            ASSERT_TRUE(completed);
            ASSERT_TRUE(measured.has_value());
            EXPECT_NEAR(measured->a.Value(), expected.a.Value(), 1e-6f);
            EXPECT_NEAR(measured->b.Value(), expected.b.Value(), 1e-6f);
            EXPECT_NEAR(measured->c.Value(), expected.c.Value(), 1e-6f);
        }

        StrictMock<services::PhaseCurrentOffsetsMock> measurementMock;
        services::ConfirmedPhaseCurrentOffsets confirmed{ measurementMock, services::ConfirmedPhaseCurrentOffsets::Config{ .tolerance = foc::Ampere{ 0.01f } } };
        infra::Function<void(std::optional<foc::PhaseCurrents>)> onWindow;
        std::optional<foc::PhaseCurrents> measured;
        bool completed{ false };
    };
}

TEST_F(TestConfirmedPhaseCurrentOffsets, two_agreeing_windows_give_their_mean_offsets_and_noise)
{
    StartMeasurement();
    EXPECT_TRUE(confirmed.IsRunning());

    CompleteWindow(Currents(0.027f, -0.018f, 0.054f), 0.038f, true);
    EXPECT_FALSE(completed);

    CompleteWindow(Currents(0.029f, -0.016f, 0.050f), 0.040f, false);

    ExpectOffsets(Currents(0.028f, -0.017f, 0.052f));
    EXPECT_NEAR(confirmed.Noise().Value(), 0.039f, 1e-6f);
    EXPECT_FALSE(confirmed.IsRunning());
}

TEST_F(TestConfirmedPhaseCurrentOffsets, a_disagreeing_first_window_is_outvoted_by_the_next_two)
{
    StartMeasurement();

    CompleteWindow(Currents(0.027f, -0.070f, 0.043f), 0.043f, true);
    CompleteWindow(Currents(0.027f, -0.018f, 0.043f), 0.038f, true);
    EXPECT_FALSE(completed);

    CompleteWindow(Currents(0.025f, -0.016f, 0.041f), 0.036f, false);

    ExpectOffsets(Currents(0.026f, -0.017f, 0.042f));
    EXPECT_NEAR(confirmed.Noise().Value(), 0.037f, 1e-6f);
}

TEST_F(TestConfirmedPhaseCurrentOffsets, a_disagreeing_second_window_is_outvoted_by_the_first_and_third)
{
    StartMeasurement();

    CompleteWindow(Currents(0.027f, -0.018f, 0.043f), 0.038f, true);
    CompleteWindow(Currents(0.027f, -0.070f, 0.043f), 0.043f, true);
    CompleteWindow(Currents(0.029f, -0.020f, 0.045f), 0.040f, false);

    ExpectOffsets(Currents(0.028f, -0.019f, 0.044f));
    EXPECT_NEAR(confirmed.Noise().Value(), 0.039f, 1e-6f);
}

TEST_F(TestConfirmedPhaseCurrentOffsets, three_windows_with_no_agreeing_pair_fail_the_measurement)
{
    StartMeasurement();

    CompleteWindow(Currents(0.0f, 0.0f, 0.0f), 0.038f, true);
    CompleteWindow(Currents(0.0f, 0.03f, 0.0f), 0.038f, true);
    CompleteWindow(Currents(0.0f, -0.03f, 0.0f), 0.038f, false);

    ASSERT_TRUE(completed);
    EXPECT_FALSE(measured.has_value());
    EXPECT_FALSE(confirmed.IsRunning());
}

TEST_F(TestConfirmedPhaseCurrentOffsets, each_phase_must_agree_within_the_tolerance)
{
    StartMeasurement();

    CompleteWindow(Currents(0.0f, 0.0f, 0.0f), 0.038f, true);
    CompleteWindow(Currents(0.0f, 0.0f, 0.0101f), 0.038f, true);
    CompleteWindow(Currents(0.0f, 0.0f, 0.0099f), 0.038f, false);

    ExpectOffsets(Currents(0.0f, 0.0f, 0.00495f));
}

TEST_F(TestConfirmedPhaseCurrentOffsets, a_failed_window_fails_the_measurement_at_once)
{
    StartMeasurement();

    CompleteWindow(Currents(0.027f, -0.018f, 0.054f), 0.038f, true);
    CompleteWindow(std::nullopt, 0.0f, false);

    ASSERT_TRUE(completed);
    EXPECT_FALSE(measured.has_value());
}

TEST_F(TestConfirmedPhaseCurrentOffsets, abort_stops_the_window_in_flight_and_reports_nothing)
{
    StartMeasurement();
    CompleteWindow(Currents(0.027f, -0.018f, 0.054f), 0.038f, true);

    EXPECT_CALL(measurementMock, Abort());
    confirmed.Abort();

    EXPECT_FALSE(confirmed.IsRunning());
    EXPECT_FALSE(completed);
}

TEST_F(TestConfirmedPhaseCurrentOffsets, abort_without_a_measurement_does_nothing)
{
    confirmed.Abort();

    EXPECT_FALSE(confirmed.IsRunning());
}

TEST_F(TestConfirmedPhaseCurrentOffsets, a_new_measurement_starts_from_no_windows)
{
    StartMeasurement();
    CompleteWindow(Currents(0.0f, -0.07f, 0.0f), 0.043f, true);
    CompleteWindow(Currents(0.0f, 0.0f, 0.0f), 0.038f, true);
    CompleteWindow(Currents(0.0f, 0.0f, 0.0f), 0.038f, false);
    completed = false;

    StartMeasurement();
    CompleteWindow(Currents(0.0f, -0.07f, 0.0f), 0.043f, true);
    CompleteWindow(Currents(0.0f, 0.0f, 0.0f), 0.038f, true);
    EXPECT_FALSE(completed);
}

TEST_F(TestConfirmedPhaseCurrentOffsets, apply_hands_the_offsets_to_the_measurement)
{
    const auto offsets = Currents(0.027f, -0.018f, 0.054f);
    EXPECT_CALL(measurementMock, Apply(_)).WillOnce([&offsets](const foc::PhaseCurrents& applied)
        {
            EXPECT_EQ(applied.b.Value(), offsets.b.Value());
        });

    confirmed.Apply(offsets);
}
