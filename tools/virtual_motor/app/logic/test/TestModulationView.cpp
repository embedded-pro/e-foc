#include "tools/virtual_motor/app/logic/ModulationView.hpp"
#include "ui/backend/recording/RecordingCanvas.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <cmath>

namespace
{
    using namespace virtual_motor;
    using ui::backend::recording::CommandKind;

    app::Capture Circle(float radius, float bus)
    {
        app::Capture capture{ 1, 0, 0, std::vector<port::Sample>(40) };
        for (std::size_t index = 0; index != capture.periods.size(); ++index)
        {
            const auto angle = static_cast<float>(index) * 0.2f;
            capture.periods[index][12] = radius * std::cos(angle);
            capture.periods[index][13] = radius * std::sin(angle);
            capture.periods[index][14] = 0.1f * std::sin(angle);
            capture.periods[index][15] = 1.5f;
            capture.periods[index][22] = bus;
        }
        return capture;
    }

    class TestModulationView
        : public testing::Test
    {
    public:
        bool Drew(const std::string& text) const
        {
            return std::ranges::any_of(canvas.Commands(), [&text](const auto& command)
                {
                    return command.kind == CommandKind::DrawText && command.text.starts_with(text);
                });
        }

        std::size_t Polylines() const
        {
            return std::ranges::count_if(canvas.Commands(), [](const auto& command)
                {
                    return command.kind == CommandKind::DrawPolyline;
                });
        }

        ui::backend::recording::RecordingCanvas canvas;
        app::ModulationView view;
    };
}

TEST(TestOvermodulation, the_linear_limit_is_the_bus_voltage_over_root_three)
{
    EXPECT_FALSE(app::IsOvermodulated(13.8f, 0.0f, 24.0f));
    EXPECT_TRUE(app::IsOvermodulated(10.0f, 10.0f, 24.0f));
    EXPECT_FALSE(app::IsOvermodulated(10.0f, 10.0f, 0.0f));
}

TEST_F(TestModulationView, draws_the_voltage_and_current_trails_over_the_span)
{
    const auto capture = Circle(10.0f, 24.0f);
    app::CaptureTrace trace{ capture, 1000 };
    view.Show(&trace, { 0.0, 0.039 }, 0.0);

    view.Paint(canvas, { 0.0f, 0.0f, 1000.0f, 500.0f });

    EXPECT_GE(Polylines(), 16u);
    EXPECT_TRUE(Drew("Id 1.65 A"));
    EXPECT_FALSE(Drew("Overmodulation"));
    EXPECT_EQ(view.Name(), "Modulation");
}

TEST_F(TestModulationView, a_vector_beyond_the_linear_circle_reads_overmodulation)
{
    const auto capture = Circle(15.0f, 24.0f);
    app::CaptureTrace trace{ capture, 1000 };
    view.Show(&trace, { 0.0, 0.039 }, 0.0);

    view.Paint(canvas, { 0.0f, 0.0f, 1000.0f, 500.0f });

    EXPECT_TRUE(Drew("Overmodulation"));
}

TEST_F(TestModulationView, without_a_source_it_draws_the_hexagon_and_axes_alone)
{
    view.Paint(canvas, { 0.0f, 0.0f, 1000.0f, 500.0f });

    EXPECT_TRUE(Drew("Current in the d-q plane"));
    EXPECT_FALSE(Drew("Overmodulation"));
    EXPECT_GT(view.MinimumSize().width, 0.0f);
}
