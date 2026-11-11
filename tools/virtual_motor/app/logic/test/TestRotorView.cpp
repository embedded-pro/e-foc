#include "tools/virtual_motor/app/logic/RotorView.hpp"
#include "ui/backend/recording/RecordingCanvas.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>

namespace
{
    using namespace virtual_motor;
    using ui::backend::recording::CommandKind;

    class TestRotorView
        : public testing::Test
    {
    public:
        TestRotorView()
        {
            capture.periods[2][0] = 2.0f;
            capture.periods[2][1] = -1.0f;
            capture.periods[2][2] = -1.0f;
            capture.periods[2][16] = 0.5f;
            capture.periods[2][17] = 0.6f;
        }

        bool Drew(const std::string& text) const
        {
            return std::ranges::any_of(canvas.Commands(), [&text](const auto& command)
                {
                    return command.kind == CommandKind::DrawText && command.text.starts_with(text);
                });
        }

        app::Capture capture{ 1, 2, 100, std::vector<port::Sample>(5) };
        app::CaptureTrace trace{ capture, 1000 };
        ui::backend::recording::RecordingCanvas canvas;
        app::RotorView view{ 4 };
    };
}

TEST(TestClarke, the_space_vector_is_amplitude_invariant)
{
    const auto vector = app::ClarkeOf(2.0f, -1.0f, -1.0f);
    EXPECT_NEAR(vector.alpha, 2.0f, 1e-6f);
    EXPECT_NEAR(vector.beta, 0.0f, 1e-6f);
    EXPECT_NEAR(app::ClarkeOf(0.0f, 1.0f, -1.0f).beta, 2.0f / std::sqrt(3.0f), 1e-6f);
}

TEST_F(TestRotorView, shows_the_instant_at_the_origin_with_its_poles_windings_and_vectors)
{
    view.Show(&trace, { -0.002, 0.002 }, 0.0);
    view.Paint(canvas, { 0.0f, 0.0f, 500.0f, 500.0f });

    ASSERT_TRUE(view.Instant().has_value());
    EXPECT_NEAR((*view.Instant())[16], 0.5f, 1e-6f);
    EXPECT_FALSE(view.InRotorFrame());
    const auto polygons = std::ranges::count_if(canvas.Commands(), [](const auto& command)
        {
            return command.kind == CommandKind::DrawPolygon;
        });
    EXPECT_EQ(polygons, 2 * 4 + 1);
    EXPECT_TRUE(Drew("I 2"));
    EXPECT_FALSE(Drew("V "));
    EXPECT_TRUE(Drew("ω 0 rad/s"));
    EXPECT_TRUE(Drew("Stationary frame"));
}

TEST_F(TestRotorView, above_a_quarter_turn_per_frame_it_draws_the_rotor_frame_and_says_so)
{
    capture.periods[2][18] = 100.0f;
    view.Show(&trace, {}, 0.0);
    view.Paint(canvas, { 0.0f, 0.0f, 500.0f, 500.0f });

    EXPECT_TRUE(view.InRotorFrame());
    EXPECT_TRUE(Drew("Rotor frame: above 39.3 rad/s"));
}

TEST_F(TestRotorView, without_a_source_it_draws_a_motor_at_rest)
{
    view.Paint(canvas, { 0.0f, 0.0f, 500.0f, 500.0f });

    EXPECT_FALSE(view.Instant().has_value());
    EXPECT_EQ(view.Name(), "Rotor");
    EXPECT_GT(view.MinimumSize().height, 0.0f);
}
