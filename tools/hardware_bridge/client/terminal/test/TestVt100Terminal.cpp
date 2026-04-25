#include "tools/hardware_bridge/client/terminal/Vt100Terminal.hpp"
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace
{
    class TestVt100Terminal : public ::testing::Test
    {
    protected:
        tool::terminal::Vt100Terminal terminal{ 6, 20 };

        void Feed(std::string_view s)
        {
            terminal.Feed(s);
        }

        std::string Line(int row) const
        {
            return terminal.Screen().LineText(row);
        }
    };
}

TEST_F(TestVt100Terminal, plain_ascii_appears_on_first_line)
{
    Feed("Hello");

    EXPECT_EQ(Line(0), "Hello");
}

TEST_F(TestVt100Terminal, crlf_starts_a_new_line_with_no_loss)
{
    Feed("Hi\r\nBye");

    EXPECT_EQ(Line(0), "Hi");
    EXPECT_EQ(Line(1), "Bye");
}

TEST_F(TestVt100Terminal, byte_chunked_crlf_does_not_lose_characters)
{
    // Feed CR then LF as separate Feed() calls (the original failure mode).
    Feed("Hi");
    Feed("\r");
    Feed("\n");
    Feed("Bye");

    EXPECT_EQ(Line(0), "Hi");
    EXPECT_EQ(Line(1), "Bye");
}

TEST_F(TestVt100Terminal, single_byte_chunked_csi_works)
{
    const std::string seq = "ab\x1B[1;5HXY";
    for (char c : seq)
        terminal.Feed(std::string_view{ &c, 1 });

    EXPECT_EQ(Line(0), "ab  XY");
}

TEST_F(TestVt100Terminal, csi_h_is_one_based_addressing)
{
    Feed("\x1B[2;3HX");

    EXPECT_EQ(Line(1), "  X");
    EXPECT_EQ(terminal.Screen().Cursor().row, 1);
    EXPECT_EQ(terminal.Screen().Cursor().column, 3);
}

TEST_F(TestVt100Terminal, csi_h_with_no_params_homes_cursor)
{
    Feed("ABCD");
    Feed("\x1B[H");

    EXPECT_EQ(terminal.Screen().Cursor().row, 0);
    EXPECT_EQ(terminal.Screen().Cursor().column, 0);
}

TEST_F(TestVt100Terminal, cursor_movements_clamp_at_edges)
{
    Feed("\x1B[10A");
    EXPECT_EQ(terminal.Screen().Cursor().row, 0);

    Feed("\x1B[10B");
    EXPECT_EQ(terminal.Screen().Cursor().row, 5);

    Feed("\x1B[100C");
    EXPECT_EQ(terminal.Screen().Cursor().column, 19);

    Feed("\x1B[100D");
    EXPECT_EQ(terminal.Screen().Cursor().column, 0);
}

TEST_F(TestVt100Terminal, erase_in_line_to_right)
{
    Feed("12345");
    Feed("\x1B[3G\x1B[K"); // move to col 3, erase to right

    EXPECT_EQ(Line(0), "12");
}

TEST_F(TestVt100Terminal, erase_in_display_clears_screen)
{
    Feed("abc\r\ndef\r\n");
    Feed("\x1B[2J");

    EXPECT_EQ(Line(0), "");
    EXPECT_EQ(Line(1), "");
}

TEST_F(TestVt100Terminal, sgr_zero_resets_attributes)
{
    Feed("\x1B[1;31mA");
    EXPECT_TRUE(terminal.Screen().At(0, 0).rendition.bold);
    EXPECT_EQ(terminal.Screen().At(0, 0).rendition.foreground, tool::terminal::Color::Red);

    Feed("\x1B[0mB");

    EXPECT_FALSE(terminal.Screen().At(0, 1).rendition.bold);
    EXPECT_EQ(terminal.Screen().At(0, 1).rendition.foreground, tool::terminal::Color::Default);
}

TEST_F(TestVt100Terminal, sgr_empty_param_means_reset)
{
    Feed("\x1B[7mA");
    Feed("\x1B[mB");

    EXPECT_FALSE(terminal.Screen().At(0, 1).rendition.inverse);
}

TEST_F(TestVt100Terminal, sgr_unknown_codes_are_ignored)
{
    Feed("\x1B[1;999;31mA");

    EXPECT_TRUE(terminal.Screen().At(0, 0).rendition.bold);
    EXPECT_EQ(terminal.Screen().At(0, 0).rendition.foreground, tool::terminal::Color::Red);
}

TEST_F(TestVt100Terminal, esc_d_indexes_cursor_down)
{
    Feed("A\x1B"
         "DB");

    EXPECT_EQ(Line(0), "A");
    EXPECT_EQ(Line(1), " B");
}

TEST_F(TestVt100Terminal, esc_e_is_carriage_return_plus_index)
{
    Feed("AB\x1B"
         "EC");

    EXPECT_EQ(Line(0), "AB");
    EXPECT_EQ(Line(1), "C");
}

TEST_F(TestVt100Terminal, esc_m_reverse_index)
{
    Feed("\r\nA\x1B"
         "MB");

    EXPECT_EQ(Line(0), " B");
    EXPECT_EQ(Line(1), "A");
}

TEST_F(TestVt100Terminal, esc_7_and_esc_8_save_restore_cursor)
{
    Feed("ABC\x1B"
         "7\r\nDEF\x1B"
         "8X");

    EXPECT_EQ(Line(0), "ABCX");
    EXPECT_EQ(Line(1), "DEF");
}

TEST_F(TestVt100Terminal, esc_c_full_reset)
{
    Feed("ABCDE\x1B"
         "c");

    EXPECT_EQ(Line(0), "");
    EXPECT_EQ(terminal.Screen().Cursor().row, 0);
    EXPECT_EQ(terminal.Screen().Cursor().column, 0);
}

TEST_F(TestVt100Terminal, esc_paren_b_designates_ascii_no_visual_effect)
{
    Feed("\x1B(BHello");

    EXPECT_EQ(Line(0), "Hello");
}

TEST_F(TestVt100Terminal, dsr_5_replies_with_zero_n)
{
    Feed("\x1B[5n");

    EXPECT_EQ(terminal.TakeOutgoing(), std::string{ "\x1B[0n" });
}

TEST_F(TestVt100Terminal, dsr_6_replies_with_cursor_position)
{
    Feed("\x1B[3;7HX");
    Feed("\x1B[6n");

    // After writing 'X' at (3,7), cursor advances to column 8 (1-based).
    EXPECT_EQ(terminal.TakeOutgoing(), std::string{ "\x1B[3;8R" });
}

TEST_F(TestVt100Terminal, da_replies_with_device_attributes)
{
    Feed("\x1B[c");

    EXPECT_EQ(terminal.TakeOutgoing(), std::string{ "\x1B[?6c" });
}

TEST_F(TestVt100Terminal, decset_decrst_toggles_autowrap)
{
    Feed("\x1B[?7l");
    EXPECT_FALSE(terminal.Screen().GetModes().autoWrap);

    Feed("\x1B[?7h");
    EXPECT_TRUE(terminal.Screen().GetModes().autoWrap);
}

TEST_F(TestVt100Terminal, lnm_set_makes_lf_perform_crlf)
{
    Feed("\x1B[20h");
    Feed("AB\nCD");

    EXPECT_EQ(Line(0), "AB");
    EXPECT_EQ(Line(1), "CD");
}

TEST_F(TestVt100Terminal, scroll_region_limits_scrolling)
{
    Feed("\x1B[2;4r");
    Feed("\x1B[4;1H");
    Feed("X\r\nY");

    EXPECT_EQ(Line(3), "Y");
}

TEST_F(TestVt100Terminal, malformed_csi_is_recovered)
{
    Feed("\x1B[zHelp");

    EXPECT_EQ(Line(0), "Help");
}

TEST_F(TestVt100Terminal, regression_help_table_with_color_and_tabs)
{
    // Simulates the kind of output the firmware CLI emits: colored
    // banner, CRLF-terminated rows, and tab-aligned columns. Feed it
    // byte-by-byte to match the worst-case chunking behavior of a
    // real serial port.
    const std::string output =
        "\x1B[32m== bridge ==\x1B[0m\r\n"
        "h\thelp\r\n"
        "q\tquit\r\n";

    for (char c : output)
        terminal.Feed(std::string_view{ &c, 1 });

    EXPECT_EQ(Line(0), "== bridge ==");
    EXPECT_EQ(Line(1), "h       help");
    EXPECT_EQ(Line(2), "q       quit");
}

TEST_F(TestVt100Terminal, embedded_control_inside_csi_does_not_lose_text)
{
    // CR embedded mid-CSI must be executed inline; the surrounding
    // sequence and following text must still arrive intact.
    Feed("AB\x1B[1\r;2H");
    Feed("X");

    // The CR moves cursor to column 0 of row 0 mid-sequence; the CSI
    // then completes as CUP(1,2) -> 0-based (0,1). 'X' overwrites
    // column 1, so Line(0) reads "AX".
    EXPECT_EQ(Line(0), "AX");
}
