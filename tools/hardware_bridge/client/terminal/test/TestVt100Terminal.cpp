#include "tools/hardware_bridge/client/terminal/Vt100Terminal.hpp"
#include <array>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <utility>

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

TEST_F(TestVt100Terminal, feed_accepts_byte_ranges)
{
    const uint8_t bytes[] = { 'O', 'K' };

    terminal.Feed(bytes, sizeof(bytes));

    EXPECT_EQ(Line(0), "OK");
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

TEST_F(TestVt100Terminal, sgr_sets_and_clears_text_style_flags)
{
    Feed("\x1B[1;2;3;4;5;7mA");
    const auto& styled = terminal.Screen().At(0, 0).rendition;
    EXPECT_TRUE(styled.bold);
    EXPECT_TRUE(styled.faint);
    EXPECT_TRUE(styled.italic);
    EXPECT_TRUE(styled.underline);
    EXPECT_TRUE(styled.blink);
    EXPECT_TRUE(styled.inverse);

    Feed("\x1B[22;23;24;25;27mB");
    const auto& cleared = terminal.Screen().At(0, 1).rendition;
    EXPECT_FALSE(cleared.bold);
    EXPECT_FALSE(cleared.faint);
    EXPECT_FALSE(cleared.italic);
    EXPECT_FALSE(cleared.underline);
    EXPECT_FALSE(cleared.blink);
    EXPECT_FALSE(cleared.inverse);
}

TEST_F(TestVt100Terminal, sgr_maps_all_foreground_colors)
{
    const std::array<std::pair<int, tool::terminal::Color>, 16> colors{ {
        { 30, tool::terminal::Color::Black },
        { 31, tool::terminal::Color::Red },
        { 32, tool::terminal::Color::Green },
        { 33, tool::terminal::Color::Yellow },
        { 34, tool::terminal::Color::Blue },
        { 35, tool::terminal::Color::Magenta },
        { 36, tool::terminal::Color::Cyan },
        { 37, tool::terminal::Color::White },
        { 90, tool::terminal::Color::BrightBlack },
        { 91, tool::terminal::Color::BrightRed },
        { 92, tool::terminal::Color::BrightGreen },
        { 93, tool::terminal::Color::BrightYellow },
        { 94, tool::terminal::Color::BrightBlue },
        { 95, tool::terminal::Color::BrightMagenta },
        { 96, tool::terminal::Color::BrightCyan },
        { 97, tool::terminal::Color::BrightWhite },
    } };

    for (std::size_t i = 0; i < colors.size(); ++i)
    {
        Feed("\x1B[" + std::to_string(colors[i].first) + "mX");
        EXPECT_EQ(terminal.Screen().At(0, static_cast<int>(i)).rendition.foreground, colors[i].second);
    }

    Feed("\x1B[39mY");
    EXPECT_EQ(terminal.Screen().At(0, 16).rendition.foreground, tool::terminal::Color::Default);
}

TEST_F(TestVt100Terminal, sgr_maps_all_background_colors)
{
    const std::array<std::pair<int, tool::terminal::Color>, 16> colors{ {
        { 40, tool::terminal::Color::Black },
        { 41, tool::terminal::Color::Red },
        { 42, tool::terminal::Color::Green },
        { 43, tool::terminal::Color::Yellow },
        { 44, tool::terminal::Color::Blue },
        { 45, tool::terminal::Color::Magenta },
        { 46, tool::terminal::Color::Cyan },
        { 47, tool::terminal::Color::White },
        { 100, tool::terminal::Color::BrightBlack },
        { 101, tool::terminal::Color::BrightRed },
        { 102, tool::terminal::Color::BrightGreen },
        { 103, tool::terminal::Color::BrightYellow },
        { 104, tool::terminal::Color::BrightBlue },
        { 105, tool::terminal::Color::BrightMagenta },
        { 106, tool::terminal::Color::BrightCyan },
        { 107, tool::terminal::Color::BrightWhite },
    } };

    for (std::size_t i = 0; i < colors.size(); ++i)
    {
        Feed("\x1B[" + std::to_string(colors[i].first) + "mX");
        EXPECT_EQ(terminal.Screen().At(0, static_cast<int>(i)).rendition.background, colors[i].second);
    }

    Feed("\x1B[49mY");
    EXPECT_EQ(terminal.Screen().At(0, 16).rendition.background, tool::terminal::Color::Default);
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

TEST_F(TestVt100Terminal, esc_hash_eight_fills_screen_with_alignment_pattern)
{
    Feed("\x1B#8");

    EXPECT_EQ(Line(0), "EEEEEEEEEEEEEEEEEEEE");
    EXPECT_EQ(Line(5), "EEEEEEEEEEEEEEEEEEEE");
    EXPECT_EQ(terminal.Screen().Cursor().row, 0);
    EXPECT_EQ(terminal.Screen().Cursor().column, 0);
}

TEST_F(TestVt100Terminal, keypad_application_mode_toggles_with_esc_sequences)
{
    Feed("\x1B=");
    EXPECT_TRUE(terminal.Screen().GetModes().applicationKeypad);

    Feed("\x1B>");
    EXPECT_FALSE(terminal.Screen().GetModes().applicationKeypad);
}

TEST_F(TestVt100Terminal, decid_replies_with_device_attributes)
{
    terminal.SetDeviceAttributesResponse("custom");

    Feed("\x1BZ");

    EXPECT_EQ(terminal.TakeOutgoing(), "custom");
    EXPECT_EQ(terminal.TakeOutgoing(), "");
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

TEST_F(TestVt100Terminal, private_da_is_ignored)
{
    Feed("\x1B[?0c");

    EXPECT_EQ(terminal.TakeOutgoing(), "");
}

TEST_F(TestVt100Terminal, decset_decrst_toggles_autowrap)
{
    Feed("\x1B[?7l");
    EXPECT_FALSE(terminal.Screen().GetModes().autoWrap);

    Feed("\x1B[?7h");
    EXPECT_TRUE(terminal.Screen().GetModes().autoWrap);
}

TEST_F(TestVt100Terminal, private_modes_toggle_cursor_keys_origin_and_visibility)
{
    Feed("\x1B[?1;6;25h");
    EXPECT_TRUE(terminal.Screen().GetModes().applicationCursorKeys);
    EXPECT_TRUE(terminal.Screen().GetModes().originMode);
    EXPECT_TRUE(terminal.Screen().GetModes().cursorVisible);

    Feed("\x1B[?1;6;25l");
    EXPECT_FALSE(terminal.Screen().GetModes().applicationCursorKeys);
    EXPECT_FALSE(terminal.Screen().GetModes().originMode);
    EXPECT_FALSE(terminal.Screen().GetModes().cursorVisible);
}

TEST_F(TestVt100Terminal, lnm_set_makes_lf_perform_crlf)
{
    Feed("\x1B[20h");
    Feed("AB\nCD");

    EXPECT_EQ(Line(0), "AB");
    EXPECT_EQ(Line(1), "CD");
}

TEST_F(TestVt100Terminal, cursor_next_and_previous_line_sequences_move_to_column_one)
{
    Feed("\x1B[3;5HX");

    Feed("\x1B[2EY");
    EXPECT_EQ(terminal.Screen().Cursor().row, 4);
    EXPECT_EQ(terminal.Screen().Cursor().column, 1);
    EXPECT_EQ(Line(4), "Y");

    Feed("\x1B[2FZ");
    EXPECT_EQ(terminal.Screen().Cursor().row, 2);
    EXPECT_EQ(terminal.Screen().Cursor().column, 1);
    EXPECT_EQ(Line(2), "Z   X");
}

TEST_F(TestVt100Terminal, hvp_moves_cursor_like_cup)
{
    Feed("\x1B[4;6fX");

    EXPECT_EQ(Line(3), "     X");
}

TEST_F(TestVt100Terminal, tab_clear_sequences_update_tab_stops)
{
    Feed("\x1B[3g");
    Feed("\tA");
    EXPECT_EQ(Line(0), "                   A");

    Feed("\x1B[1;5H\x1BH\x1B[0g\x1B[1;1H\tB");
    EXPECT_EQ(terminal.Screen().Cursor().column, 19);
}

TEST_F(TestVt100Terminal, save_and_restore_cursor_with_csi_sequences)
{
    Feed("AB\x1B[s\r\nCD\x1B[uX");

    EXPECT_EQ(Line(0), "ABX");
    EXPECT_EQ(Line(1), "CD");
}

TEST_F(TestVt100Terminal, vertical_tab_and_form_feed_are_treated_as_line_feed)
{
    Feed("A\vB\fC");

    EXPECT_EQ(Line(0), "A");
    EXPECT_EQ(Line(1), " B");
    EXPECT_EQ(Line(2), "  C");
}

TEST_F(TestVt100Terminal, ignored_execute_bytes_have_no_visual_effect)
{
    Feed(std::string_view{ "A\0\x05\x11\x13\x07"
                           "B",
        7 });

    EXPECT_EQ(Line(0), "AB");
}

TEST_F(TestVt100Terminal, csi_with_intermediate_is_ignored)
{
    Feed("A\x1B[1 qB");

    EXPECT_EQ(Line(0), "AB");
}

TEST_F(TestVt100Terminal, osc_payload_is_accepted_and_ignored)
{
    Feed("A\x1B]0;title\x07"
         "B");

    EXPECT_EQ(Line(0), "AB");
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
