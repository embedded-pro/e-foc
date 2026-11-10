#include "tools/virtual_motor/app/logic/TimeAxis.hpp"
#include "tools/virtual_motor/app/logic/TraceView.hpp"
#include "ui/backend/recording/RecordingCanvas.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <ranges>

namespace
{
    using namespace virtual_motor;
    using ui::backend::recording::CommandKind;

    const app::ViewSpec& Supply()
    {
        return app::TraceViews()[3];
    }

    const app::ViewSpec& Currents()
    {
        return app::TraceViews()[0];
    }

    port::Window WindowAt(uint32_t sequence, float low, float high)
    {
        port::Window window{ sequence, uint64_t{ sequence } * 10, 10, {}, {} };
        window.minimum.fill(low);
        window.maximum.fill(high);
        return window;
    }

    class TestTraceView
        : public testing::Test
    {
    public:
        std::size_t Polygons() const
        {
            return std::ranges::count_if(canvas.Commands(), [](const auto& command)
                {
                    return command.kind == CommandKind::DrawPolygon;
                });
        }

        void Paint(app::TraceView& view)
        {
            canvas.Clear();
            view.Paint(canvas, bounds);
        }

        ui::backend::recording::RecordingCanvas canvas;
        ui::Rect bounds{ 0.0f, 0.0f, 600.0f, 400.0f };
        app::TraceHistory history{ 100 };
        app::HistoryTrace trace{ history, 20000 };
    };
}

TEST_F(TestTraceView, draws_one_envelope_per_trace_that_has_data)
{
    for (uint32_t sequence = 0; sequence != 20; ++sequence)
        history.Add(WindowAt(sequence, -1.0f, 1.0f));
    app::TraceView view{ Supply() };
    view.Show(&trace, { 0.0, 0.01 }, 0.01);

    Paint(view);

    EXPECT_EQ(Polygons(), 3u);
}

TEST_F(TestTraceView, a_dropped_window_breaks_the_trace)
{
    for (uint32_t sequence : { 0u, 1u, 2u, 5u, 6u, 7u })
        history.Add(WindowAt(sequence, 0.0f, 1.0f));
    app::TraceView view{ Supply() };
    view.Show(&trace, { 0.0, 0.004 }, 0.004);

    Paint(view);

    EXPECT_EQ(Polygons(), 6u);
}

TEST_F(TestTraceView, the_envelope_spans_the_lowest_minimum_to_the_highest_maximum_inside_the_panel)
{
    history.Add(WindowAt(0, -2.0f, 1.0f));
    history.Add(WindowAt(1, -1.0f, 3.0f));
    app::TraceView view{ Supply() };
    view.Show(&trace, { 0.0, 0.001 }, 0.001);

    Paint(view);

    const auto polygon = std::ranges::find_if(canvas.Commands(), [](const auto& command)
        {
            return command.kind == CommandKind::DrawPolygon;
        });
    ASSERT_NE(polygon, canvas.Commands().end());
    const auto [top, bottom] = std::ranges::minmax(polygon->points | std::views::transform([](const ui::Point& point)
                                                                         {
                                                                             return point.y;
                                                                         }));
    EXPECT_GT(top, 0.0f);
    EXPECT_LT(bottom, 400.0f);
    EXPECT_GT(bottom - top, 50.0f);
}

TEST_F(TestTraceView, cursors_read_the_traces_and_the_time_between_them)
{
    app::Capture capture{ 1, 2, 0, std::vector<port::Sample>(6) };
    for (std::size_t index = 0; index != 6; ++index)
        capture.periods[index][0] = static_cast<float>(index);
    app::CaptureTrace captureTrace{ capture, 1000 };
    app::TraceView view{ Currents() };
    view.Show(&captureTrace, { -0.002, 0.003 }, 0.0);
    view.SetCursorsEnabled(true);
    Paint(view);

    std::size_t changes{ 0 };
    view.OnCursorsChanged([&changes]()
        {
            ++changes;
        });
    view.OnMousePress({ { 88.0f + 1.0f, 100.0f }, ui::MouseButton::Left, {} });
    view.OnMousePress({ { 88.0f + 500.0f * 0.8f + 1.0f, 100.0f }, ui::MouseButton::Right, {} });

    EXPECT_EQ(changes, 2u);
    ASSERT_TRUE(view.CursorA().has_value());
    ASSERT_TRUE(view.CursorB().has_value());
    const auto readings = view.Readings(0);
    ASSERT_EQ(readings.size(), 6u);
    EXPECT_NEAR(*readings[0].atA, 0.0f, 1e-6f);
    EXPECT_NEAR(*readings[0].atB, 4.0f, 1e-6f);
    EXPECT_NE(view.CursorSummary().find("Δt 4"), std::string::npos);

    view.OnMouseDoubleClick({});
    EXPECT_FALSE(view.CursorA().has_value());
    EXPECT_EQ(changes, 3u);
}

TEST_F(TestTraceView, cursors_are_ignored_while_disabled)
{
    history.Add(WindowAt(0, 0.0f, 1.0f));
    app::TraceView view{ Supply() };
    view.Show(&trace, { 0.0, 0.001 }, 0.001);
    Paint(view);

    view.OnMousePress({ { 200.0f, 100.0f }, ui::MouseButton::Left, {} });

    EXPECT_FALSE(view.CursorA().has_value());
    EXPECT_TRUE(view.CursorSummary().empty());
}

TEST(TestTimeAxis, ticks_fall_on_round_times_labelled_from_the_origin)
{
    const auto ticks = virtual_motor::app::TimeTicks(0.8, 1.0, 1.0, 5);

    ASSERT_EQ(ticks.size(), 5u);
    EXPECT_NEAR(ticks.front().time, 0.8, 1e-9);
    EXPECT_EQ(ticks.front().label, "-200 ms");
    EXPECT_EQ(ticks.back().label, "0 s");
}

TEST(TestTimeAxis, times_read_in_the_unit_that_suits_them)
{
    EXPECT_EQ(virtual_motor::app::TimeText(0.0), "0 s");
    EXPECT_EQ(virtual_motor::app::TimeText(50e-6), "50 µs");
    EXPECT_EQ(virtual_motor::app::TimeText(-0.0125), "-12.5 ms");
    EXPECT_EQ(virtual_motor::app::TimeText(2.5), "2.5 s");
}

TEST_F(TestTraceView, axis_labels_carry_the_unit_and_no_float_noise)
{
    history.Add(WindowAt(0, -0.5f, 0.5f));
    history.Add(WindowAt(1, -0.5f, 0.5f));
    app::TraceView view{ Supply() };
    view.Show(&trace, { 0.0, 0.001 }, 0.001);

    Paint(view);

    std::vector<std::string> labels;
    for (const auto& command : canvas.Commands())
        if (command.kind == CommandKind::DrawText && command.text.ends_with(" V"))
            labels.push_back(command.text);
    EXPECT_THAT(labels, testing::Contains("0.00 V"));
    EXPECT_THAT(labels, testing::Contains("0.28 V"));
    EXPECT_THAT(labels, testing::Not(testing::Contains(testing::HasSubstr("e-"))));
}
