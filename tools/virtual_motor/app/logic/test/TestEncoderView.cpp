#include "tools/virtual_motor/app/logic/EncoderView.hpp"
#include "ui/backend/recording/RecordingCanvas.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <numeric>

namespace
{
    using namespace virtual_motor;
    using ui::backend::recording::CommandKind;

    app::Capture Turning(float encoderOffset, bool stuck)
    {
        app::Capture capture{ 1, 0, 0, std::vector<port::Sample>(400) };
        for (std::size_t index = 0; index != capture.periods.size(); ++index)
        {
            const auto angle = 0.001f * static_cast<float>(index);
            capture.periods[index][16] = angle;
            capture.periods[index][17] = stuck && index > 100 ? 0.1f : angle - encoderOffset + (index % 2 == 0 ? 0.001f : -0.001f);
        }
        return capture;
    }

    bool Drew(const ui::backend::recording::RecordingCanvas& canvas, const std::string& text)
    {
        return std::ranges::any_of(canvas.Commands(), [&text](const auto& command)
            {
                return command.kind == CommandKind::DrawText && command.text.starts_with(text);
            });
    }
}

TEST(TestEncoderStatistics, the_difference_is_in_electrical_degrees_wrapped_to_a_half_turn)
{
    EXPECT_NEAR(app::ElectricalDifferenceDegrees(0.01f, 0.0f, 4), 0.04f * 180.0f / 3.14159265f, 1e-3f);
    EXPECT_NEAR(app::ElectricalDifferenceDegrees(1.0f, 0.0f, 4), (4.0f - 2.0f * 3.14159265f) * 180.0f / 3.14159265f, 1e-2f);
}

TEST(TestEncoderStatistics, bias_and_noise_come_from_the_mean_and_spread_of_the_difference)
{
    const auto capture = Turning(0.002f, false);
    app::CaptureTrace trace{ capture, 20000 };

    const auto statistics = app::MeasureEncoder(trace, { 0.0, 0.02 }, { 4, 4000 });

    ASSERT_EQ(statistics.difference.size(), 400u);
    EXPECT_NEAR(statistics.mean, app::ElectricalDifferenceDegrees(0.002f, 0.0f, 4), 0.01f);
    EXPECT_NEAR(statistics.deviation, app::ElectricalDifferenceDegrees(0.001f, 0.0f, 4), 0.01f);
    EXPECT_EQ(std::accumulate(statistics.histogram.begin(), statistics.histogram.end(), std::size_t{ 0 }), 400u);
    EXPECT_FALSE(statistics.stuck);
}

TEST(TestEncoderStatistics, an_encoder_that_stopped_while_the_rotor_turns_is_stuck)
{
    const auto capture = Turning(0.0f, true);
    app::CaptureTrace trace{ capture, 20000 };

    EXPECT_TRUE(app::MeasureEncoder(trace, { 0.0, 0.02 }, { 4, 4000 }).stuck);
    EXPECT_TRUE(app::MeasureEncoder(trace, { 0.0, 0.02 }, { 4, 0 }).stuck);
    EXPECT_FALSE(app::MeasureEncoder(trace, { 0.0, 0.005 }, { 4, 4000 }).stuck);
}

TEST(TestEncoderView, shows_the_difference_its_histogram_and_the_readings)
{
    const auto capture = Turning(0.002f, false);
    app::CaptureTrace trace{ capture, 20000 };
    app::EncoderView view{ { 4, 4000 } };
    ui::backend::recording::RecordingCanvas canvas;

    view.Show(&trace, { 0.0, 0.02 }, 0.0);
    view.Paint(canvas, { 0.0f, 0.0f, 800.0f, 500.0f });

    EXPECT_TRUE(Drew(canvas, "Bias (mean)  0.458°e"));
    EXPECT_TRUE(Drew(canvas, "4000 counts per revolution · resolution 0.36°e"));
    EXPECT_TRUE(Drew(canvas, "Following the rotor"));
    EXPECT_EQ(view.Name(), "Encoder");
    EXPECT_GT(view.MinimumSize().width, 0.0f);
}

TEST(TestEncoderView, without_a_source_it_reads_no_difference)
{
    app::EncoderView view{ { 4, 0 } };
    ui::backend::recording::RecordingCanvas canvas;

    view.Show(nullptr, {}, 0.0);
    view.Paint(canvas, { 0.0f, 0.0f, 800.0f, 500.0f });

    EXPECT_TRUE(view.Statistics().difference.empty());
    EXPECT_TRUE(Drew(canvas, "Ideal encoder: no quantisation"));
}
