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
    wide.ClearAllTabStops();

    wide.MoveCursor(1, 4);
    wide.SetTabStop();
    wide.MoveCursor(1, 1);
    wide.HorizontalTab();

    EXPECT_EQ(wide.Cursor().column, 3);
}

TEST_F(TestTerminalScreen, reverse_index_at_top_scrolls_down)
{
    screen.MoveCursor(2, 1);
    screen.Write(U'X');
    screen.MoveCursor(1, 1);

    screen.ReverseIndex();

    EXPECT_EQ(screen.Cursor().row, 0);
    EXPECT_EQ(screen.LineText(2), "X");
    screen.ReverseIndex(); // already at top -> scroll down inserts blank
    EXPECT_EQ(screen.LineText(0), "");
}

TEST_F(TestTerminalScreen, save_restore_cursor)
{
    screen.MoveCursor(3, 5);
    screen.SaveCursor();
    screen.MoveCursor(1, 1);

    screen.RestoreCursor();

    EXPECT_EQ(screen.Cursor().row, 2);
    EXPECT_EQ(screen.Cursor().column, 4);
}

TEST_F(TestTerminalScreen, erase_in_line_to_right)
{
    for (int c = 0; c < 5; ++c)
        screen.Write(U'X');
    screen.MoveCursor(1, 3);

    screen.EraseInLine(0);

    EXPECT_EQ(screen.LineText(0), "XX");
}

TEST_F(TestTerminalScreen, erase_in_line_to_left)
{
    for (int c = 0; c < 5; ++c)
        screen.Write(U'X');
    screen.MoveCursor(1, 3);

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
        screen.MoveCursor(r + 1, 1);
        screen.Write(U'A');
    }
    screen.MoveCursor(1, 1);

    screen.EraseInDisplay(0);

    EXPECT_EQ(screen.LineText(0), "");
    EXPECT_EQ(screen.LineText(1), "");
    EXPECT_EQ(screen.LineText(2), "");
}

TEST_F(TestTerminalScreen, erase_in_display_all)
{
    screen.Write(U'X');
    screen.MoveCursor(3, 3);
    screen.Write(U'Y');

    screen.EraseInDisplay(2);

    EXPECT_EQ(screen.LineText(0), "");
    EXPECT_EQ(screen.LineText(2), "");
}

TEST_F(TestTerminalScreen, move_cursor_clamps_to_screen)
{
    screen.MoveCursor(99, 99);

    EXPECT_EQ(screen.Cursor().row, 4);
    EXPECT_EQ(screen.Cursor().column, 9);
}

TEST_F(TestTerminalScreen, scroll_region_limits_index_to_region)
{
    screen.SetScrollRegion(2, 4);
    screen.MoveCursor(4, 1);
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
