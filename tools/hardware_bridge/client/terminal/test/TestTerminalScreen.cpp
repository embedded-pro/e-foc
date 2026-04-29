#include "tools/hardware_bridge/client/terminal/TerminalScreen.hpp"
#include <gtest/gtest.h>

namespace
{
    class TestTerminalScreen : public ::testing::Test
    {
    protected:
        tool::terminal::TerminalScreen screen{ 5, 10 };
    };
}

TEST_F(TestTerminalScreen, default_size_is_correct)
{
    tool::terminal::TerminalScreen def;

    EXPECT_EQ(def.Rows(), 24);
    EXPECT_EQ(def.Cols(), 100);
}

TEST_F(TestTerminalScreen, invalid_size_uses_public_defaults)
{
    tool::terminal::TerminalScreen invalid{ 0, -1 };

    EXPECT_EQ(invalid.Rows(), 24);
    EXPECT_EQ(invalid.Cols(), 100);
}

TEST_F(TestTerminalScreen, const_modes_accessor_returns_mode_state)
{
    screen.GetModes().cursorVisible = false;
    const tool::terminal::TerminalScreen& constScreen = screen;

    EXPECT_FALSE(constScreen.GetModes().cursorVisible);
}

TEST_F(TestTerminalScreen, write_advances_cursor)
{
    screen.Write(U'A');
    screen.Write(U'B');

    EXPECT_EQ(screen.Cursor().row, 0);
    EXPECT_EQ(screen.Cursor().column, 2);
    EXPECT_EQ(screen.At(0, 0).codepoint, U'A');
    EXPECT_EQ(screen.At(0, 1).codepoint, U'B');
}

TEST_F(TestTerminalScreen, write_at_last_column_defers_wrap)
{
    for (int i = 0; i < 10; ++i)
        screen.Write(static_cast<char32_t>('a' + i));

    EXPECT_EQ(screen.Cursor().row, 0);
    EXPECT_EQ(screen.Cursor().column, 9);
    EXPECT_EQ(screen.At(0, 9).codepoint, U'j');

    screen.Write(U'k');

    EXPECT_EQ(screen.Cursor().row, 1);
    EXPECT_EQ(screen.Cursor().column, 1);
    EXPECT_EQ(screen.At(1, 0).codepoint, U'k');
}

TEST_F(TestTerminalScreen, carriage_return_moves_to_column_zero)
{
    screen.Write(U'X');
    screen.Write(U'Y');

    screen.CarriageReturn();

    EXPECT_EQ(screen.Cursor().column, 0);
    EXPECT_EQ(screen.Cursor().row, 0);
}

TEST_F(TestTerminalScreen, line_feed_moves_down_without_changing_column)
{
    screen.Write(U'A');
    screen.Write(U'B');

    screen.LineFeed();

    EXPECT_EQ(screen.Cursor().row, 1);
    EXPECT_EQ(screen.Cursor().column, 2);
}

TEST_F(TestTerminalScreen, crlf_writes_one_line_break_only)
{
    screen.Write(U'H');
    screen.Write(U'i');
    screen.CarriageReturn();
    screen.LineFeed();
    screen.Write(U'B');
    screen.Write(U'y');
    screen.Write(U'e');

    EXPECT_EQ(screen.LineText(0), "Hi");
    EXPECT_EQ(screen.LineText(1), "Bye");
}

TEST_F(TestTerminalScreen, line_feed_at_bottom_scrolls_screen_up)
{
    for (int r = 0; r < 5; ++r)
    {
        screen.Write(static_cast<char32_t>('A' + r));
        if (r < 4)
        {
            screen.CarriageReturn();
            screen.LineFeed();
        }
    }

    screen.CarriageReturn();
    screen.LineFeed();
    screen.Write(U'F');

    EXPECT_EQ(screen.LineText(0), "B");
    EXPECT_EQ(screen.LineText(4), "F");
    ASSERT_EQ(screen.History().size(), 1u);
    EXPECT_EQ(screen.History().back()[0].codepoint, U'A');
}

TEST_F(TestTerminalScreen, backspace_does_not_pass_left_margin)
{
    screen.Backspace();

    EXPECT_EQ(screen.Cursor().column, 0);

    screen.Write(U'A');
    screen.Backspace();

    EXPECT_EQ(screen.Cursor().column, 0);
}

TEST_F(TestTerminalScreen, horizontal_tab_advances_to_next_tab_stop)
{
    tool::terminal::TerminalScreen wide{ 5, 20 };

    wide.HorizontalTab();
    EXPECT_EQ(wide.Cursor().column, 8);

    wide.HorizontalTab();
    EXPECT_EQ(wide.Cursor().column, 16);

    wide.HorizontalTab();
    EXPECT_EQ(wide.Cursor().column, 19);
}

TEST_F(TestTerminalScreen, set_and_clear_tab_stops)
{
    tool::terminal::TerminalScreen wide{ 5, 20 };
    wide.TabStops().ClearAll();

    wide.CursorOperations().MoveTo(1, 4);
    wide.TabStops().SetHere();
    wide.CursorOperations().MoveTo(1, 1);
    wide.HorizontalTab();

    EXPECT_EQ(wide.Cursor().column, 3);

    wide.TabStops().ClearHere();
    wide.CursorOperations().MoveTo(1, 1);
    wide.HorizontalTab();

    EXPECT_EQ(wide.Cursor().column, 19);
}

TEST_F(TestTerminalScreen, clear_tab_stop_here_ignores_out_of_range_cursor_after_clamp)
{
    screen.CursorOperations().MoveTo(100, 100);

    screen.TabStops().ClearHere();

    EXPECT_EQ(screen.Cursor().row, 4);
    EXPECT_EQ(screen.Cursor().column, 9);
}

TEST_F(TestTerminalScreen, reverse_index_at_top_scrolls_down)
{
    screen.CursorOperations().MoveTo(2, 1);
    screen.Write(U'X');
    screen.CursorOperations().MoveTo(1, 1);

    screen.ReverseIndex();

    EXPECT_EQ(screen.Cursor().row, 0);
    EXPECT_EQ(screen.LineText(2), "X");
    screen.ReverseIndex(); // already at top -> scroll down inserts blank
    EXPECT_EQ(screen.LineText(0), "");
}

TEST_F(TestTerminalScreen, save_restore_cursor)
{
    screen.CursorOperations().MoveTo(3, 5);
    screen.CursorOperations().Save();
    screen.CursorOperations().MoveTo(1, 1);

    screen.CursorOperations().Restore();

    EXPECT_EQ(screen.Cursor().row, 2);
    EXPECT_EQ(screen.Cursor().column, 4);
}

TEST_F(TestTerminalScreen, erase_in_line_to_right)
{
    for (int c = 0; c < 5; ++c)
        screen.Write(U'X');
    screen.CursorOperations().MoveTo(1, 3);

    screen.EraseInLine(0);

    EXPECT_EQ(screen.LineText(0), "XX");
}

TEST_F(TestTerminalScreen, erase_in_line_to_left)
{
    for (int c = 0; c < 5; ++c)
        screen.Write(U'X');
    screen.CursorOperations().MoveTo(1, 3);

    screen.EraseInLine(1);

    EXPECT_EQ(screen.At(0, 0).codepoint, U' ');
    EXPECT_EQ(screen.At(0, 2).codepoint, U' ');
    EXPECT_EQ(screen.At(0, 3).codepoint, U'X');
}

TEST_F(TestTerminalScreen, erase_in_line_all)
{
    for (int c = 0; c < 5; ++c)
        screen.Write(U'X');

    screen.EraseInLine(2);

    EXPECT_EQ(screen.LineText(0), "");
}

TEST_F(TestTerminalScreen, erase_in_display_below)
{
    for (int r = 0; r < 3; ++r)
    {
        screen.CursorOperations().MoveTo(r + 1, 1);
        screen.Write(U'A');
    }
    screen.CursorOperations().MoveTo(1, 1);

    screen.EraseInDisplay(0);

    EXPECT_EQ(screen.LineText(0), "");
    EXPECT_EQ(screen.LineText(1), "");
    EXPECT_EQ(screen.LineText(2), "");
}

TEST_F(TestTerminalScreen, erase_in_display_all)
{
    screen.Write(U'X');
    screen.CursorOperations().MoveTo(3, 3);
    screen.Write(U'Y');

    screen.EraseInDisplay(2);

    EXPECT_EQ(screen.LineText(0), "");
    EXPECT_EQ(screen.LineText(2), "");
}

TEST_F(TestTerminalScreen, erase_in_display_above_preserves_below_cursor)
{
    screen.CursorOperations().MoveTo(1, 1);
    screen.Write(U'A');
    screen.CursorOperations().MoveTo(3, 1);
    screen.Write(U'B');
    screen.CursorOperations().MoveTo(5, 1);
    screen.Write(U'C');
    screen.CursorOperations().MoveTo(3, 1);

    screen.EraseInDisplay(1);

    EXPECT_EQ(screen.LineText(0), "");
    EXPECT_EQ(screen.LineText(2), "");
    EXPECT_EQ(screen.LineText(4), "C");
}

TEST_F(TestTerminalScreen, erase_in_display_mode_three_clears_scrollback)
{
    for (int row = 0; row < 5; ++row)
    {
        screen.Write(static_cast<char32_t>('A' + row));
        screen.CarriageReturn();
        screen.LineFeed();
    }
    ASSERT_FALSE(screen.History().empty());

    screen.EraseInDisplay(3);

    EXPECT_TRUE(screen.History().empty());
    EXPECT_EQ(screen.LineText(0), "");
}

TEST_F(TestTerminalScreen, move_cursor_clamps_to_screen)
{
    screen.CursorOperations().MoveTo(99, 99);

    EXPECT_EQ(screen.Cursor().row, 4);
    EXPECT_EQ(screen.Cursor().column, 9);
}

TEST_F(TestTerminalScreen, move_cursor_clamps_low_values_to_origin)
{
    screen.CursorOperations().MoveTo(0, -4);

    EXPECT_EQ(screen.Cursor().row, 0);
    EXPECT_EQ(screen.Cursor().column, 0);
}

TEST_F(TestTerminalScreen, cursor_movement_treats_non_positive_counts_as_one)
{
    screen.CursorOperations().MoveTo(3, 3);

    screen.CursorOperations().Up(0);
    EXPECT_EQ(screen.Cursor().row, 1);
    screen.CursorOperations().Down(-4);
    EXPECT_EQ(screen.Cursor().row, 2);
    screen.CursorOperations().Forward(0);
    EXPECT_EQ(screen.Cursor().column, 3);
    screen.CursorOperations().Backward(-9);
    EXPECT_EQ(screen.Cursor().column, 2);
}

TEST_F(TestTerminalScreen, cursor_column_clamps_low_and_high_values)
{
    screen.CursorOperations().MoveToColumn(0);
    EXPECT_EQ(screen.Cursor().column, 0);

    screen.CursorOperations().MoveToColumn(99);
    EXPECT_EQ(screen.Cursor().column, 9);
}

TEST_F(TestTerminalScreen, invalid_scroll_region_resets_to_full_screen)
{
    screen.SetScrollRegion(4, 2);

    EXPECT_EQ(screen.ScrollTop(), 0);
    EXPECT_EQ(screen.ScrollBottom(), 4);
    EXPECT_EQ(screen.Cursor().row, 0);
    EXPECT_EQ(screen.Cursor().column, 0);
}

TEST_F(TestTerminalScreen, scroll_region_clamps_bottom_to_screen)
{
    screen.SetScrollRegion(2, 99);

    EXPECT_EQ(screen.ScrollTop(), 1);
    EXPECT_EQ(screen.ScrollBottom(), 4);
}

TEST_F(TestTerminalScreen, scroll_region_clamps_top_to_first_row)
{
    screen.SetScrollRegion(0, 3);

    EXPECT_EQ(screen.ScrollTop(), 0);
    EXPECT_EQ(screen.ScrollBottom(), 2);
}

TEST_F(TestTerminalScreen, scroll_region_limits_index_to_region)
{
    screen.SetScrollRegion(2, 4);
    screen.CursorOperations().MoveTo(4, 1);
    screen.Write(U'A');
    screen.CarriageReturn();
    screen.Index();
    screen.Write(U'B');

    // Index at scroll bottom shifts rows in the region up: 'A' moves
    // from row 3 to row 2 (0-based), and 'B' is written at the new
    // bottom row 3.
    EXPECT_EQ(screen.LineText(2), "A");
    EXPECT_EQ(screen.LineText(3), "B");
}

TEST_F(TestTerminalScreen, line_text_strips_trailing_spaces)
{
    screen.Write(U'a');
    screen.Write(U' ');
    screen.Write(U'b');

    EXPECT_EQ(screen.LineText(0), "a b");
}

TEST_F(TestTerminalScreen, line_text_replaces_non_ascii_with_question_mark)
{
    screen.Write(U'\u03a9');

    EXPECT_EQ(screen.LineText(0), "?");
}

TEST_F(TestTerminalScreen, soft_reset_preserves_screen_and_history_but_resets_modes_and_rendition)
{
    tool::terminal::Rendition rendition;
    rendition.bold = true;
    screen.SetRendition(rendition);
    screen.GetModes().lineFeedNewLine = true;
    screen.Write(U'X');
    for (int row = 0; row < 5; ++row)
    {
        screen.CarriageReturn();
        screen.LineFeed();
    }
    ASSERT_FALSE(screen.History().empty());

    screen.SoftReset();

    EXPECT_EQ(screen.CurrentRendition(), tool::terminal::Rendition{});
    EXPECT_FALSE(screen.GetModes().lineFeedNewLine);
    EXPECT_FALSE(screen.History().empty());
    EXPECT_EQ(screen.LineText(0), "");
}

TEST_F(TestTerminalScreen, clear_history_removes_scrollback)
{
    for (int row = 0; row < 5; ++row)
    {
        screen.Write(static_cast<char32_t>('A' + row));
        screen.CarriageReturn();
        screen.LineFeed();
    }
    ASSERT_FALSE(screen.History().empty());

    screen.ClearHistory();

    EXPECT_TRUE(screen.History().empty());
}

TEST_F(TestTerminalScreen, scrollback_history_is_capped)
{
    for (int row = 0; row < 1005; ++row)
    {
        screen.Write(U'X');
        screen.CarriageReturn();
        screen.LineFeed();
    }

    EXPECT_EQ(screen.History().size(), 1000u);
}

TEST_F(TestTerminalScreen, disabled_autowrap_overwrites_last_column)
{
    screen.GetModes().autoWrap = false;
    for (int i = 0; i < 10; ++i)
        screen.Write(static_cast<char32_t>('0' + i));

    screen.Write(U'X');

    EXPECT_EQ(screen.Cursor().row, 0);
    EXPECT_EQ(screen.Cursor().column, 9);
    EXPECT_EQ(screen.At(0, 9).codepoint, U'X');
}
