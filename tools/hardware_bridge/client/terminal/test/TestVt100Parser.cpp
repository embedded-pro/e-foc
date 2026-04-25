#include "tools/hardware_bridge/client/terminal/Vt100Parser.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace
{
    struct ParserEvent
    {
        enum class Kind
        {
            Print,
            Execute,
            Esc,
            Csi,
            Osc
        };
        Kind kind;
        char32_t printChar{ 0 };
        uint8_t executeByte{ 0 };
        char finalByte{ 0 };
        char intermediate{ 0 };
        bool privateMarker{ false };
        std::vector<int> params;
        std::string osc;
    };

    class TestVt100Parser : public ::testing::Test
    {
    protected:
        std::vector<ParserEvent> events;

        tool::terminal::ParserCallbacks MakeCallbacks()
        {
            return tool::terminal::ParserCallbacks{
                [this](char32_t c)
                {
                    events.push_back({ ParserEvent::Kind::Print, c, 0, 0, 0, false, {}, {} });
                },
                [this](uint8_t b)
                {
                    events.push_back({ ParserEvent::Kind::Execute, 0, b, 0, 0, false, {}, {} });
                },
                [this](char f, char i)
                {
                    events.push_back({ ParserEvent::Kind::Esc, 0, 0, f, i, false, {}, {} });
                },
                [this](char f, const std::vector<int>& p, bool pm, char i)
                {
                    events.push_back({ ParserEvent::Kind::Csi, 0, 0, f, i, pm, p, {} });
                },
                [this](const std::string& s)
                {
                    events.push_back({ ParserEvent::Kind::Osc, 0, 0, 0, 0, false, {}, s });
                },
            };
        }
    };
}

TEST_F(TestVt100Parser, prints_plain_ascii_as_codepoints)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "Hi" });

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Print);
    EXPECT_EQ(events[0].printChar, U'H');
    EXPECT_EQ(events[1].printChar, U'i');
}

TEST_F(TestVt100Parser, feed_accepts_byte_ranges)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());
    const uint8_t bytes[] = { 'O', 'K' };

    parser.Feed(bytes, sizeof(bytes));

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].printChar, U'O');
    EXPECT_EQ(events[1].printChar, U'K');
}

TEST_F(TestVt100Parser, parser_with_empty_callbacks_accepts_all_event_types)
{
    tool::terminal::Vt100Parser parser(tool::terminal::ParserCallbacks{});

    parser.Feed(std::string_view{ "A\r\x1B"
                                  "D\x1B[1;2H\x1B]0;x\x07" });

    SUCCEED();
}

TEST_F(TestVt100Parser, executes_c0_controls_individually)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\r\n\b\t\x07" });

    ASSERT_EQ(events.size(), 5u);
    EXPECT_EQ(events[0].executeByte, 0x0D);
    EXPECT_EQ(events[1].executeByte, 0x0A);
    EXPECT_EQ(events[2].executeByte, 0x08);
    EXPECT_EQ(events[3].executeByte, 0x09);
    EXPECT_EQ(events[4].executeByte, 0x07);
}

TEST_F(TestVt100Parser, executes_less_common_c0_controls)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());
    const uint8_t bytes[] = { 0x19, 0x1C };

    parser.Feed(bytes, sizeof(bytes));

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].executeByte, 0x19);
    EXPECT_EQ(events[1].executeByte, 0x1C);
}

TEST_F(TestVt100Parser, ignores_del_and_nul_at_ground)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.FeedByte(0x7F);
    parser.FeedByte('A');

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Print);
    EXPECT_EQ(events[0].printChar, U'A');
}

TEST_F(TestVt100Parser, dispatches_simple_esc_final)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B"
                                  "D" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Esc);
    EXPECT_EQ(events[0].finalByte, 'D');
    EXPECT_EQ(events[0].intermediate, 0);
}

TEST_F(TestVt100Parser, dispatches_esc_with_intermediate)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B(B" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Esc);
    EXPECT_EQ(events[0].finalByte, 'B');
    EXPECT_EQ(events[0].intermediate, '(');
}

TEST_F(TestVt100Parser, lone_string_terminator_escape_is_ignored)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B\\A" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Print);
    EXPECT_EQ(events[0].printChar, U'A');
}

TEST_F(TestVt100Parser, dispatches_csi_with_no_params)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B[H" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Csi);
    EXPECT_EQ(events[0].finalByte, 'H');
    EXPECT_TRUE(events[0].params.empty());
    EXPECT_FALSE(events[0].privateMarker);
}

TEST_F(TestVt100Parser, dispatches_csi_with_multiple_params)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B[12;34H" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].finalByte, 'H');
    ASSERT_EQ(events[0].params.size(), 2u);
    EXPECT_EQ(events[0].params[0], 12);
    EXPECT_EQ(events[0].params[1], 34);
}

TEST_F(TestVt100Parser, treats_colon_csi_separators_as_semicolons)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B[12:34H" });

    ASSERT_EQ(events.size(), 1u);
    ASSERT_EQ(events[0].params.size(), 2u);
    EXPECT_EQ(events[0].params[0], 12);
    EXPECT_EQ(events[0].params[1], 34);
}

TEST_F(TestVt100Parser, treats_omitted_csi_param_as_zero)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B[;5H" });

    ASSERT_EQ(events.size(), 1u);
    ASSERT_EQ(events[0].params.size(), 2u);
    EXPECT_EQ(events[0].params[0], 0);
    EXPECT_EQ(events[0].params[1], 5);
}

TEST_F(TestVt100Parser, parses_private_marker_csi)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B[?25h" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].finalByte, 'h');
    EXPECT_TRUE(events[0].privateMarker);
    ASSERT_EQ(events[0].params.size(), 1u);
    EXPECT_EQ(events[0].params[0], 25);
}

TEST_F(TestVt100Parser, non_question_private_markers_are_tolerated_without_private_flag)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B[>0c" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].finalByte, 'c');
    EXPECT_FALSE(events[0].privateMarker);
    ASSERT_EQ(events[0].params.size(), 1u);
    EXPECT_EQ(events[0].params[0], 0);
}

TEST_F(TestVt100Parser, csi_intermediate_is_dispatched)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B[1 q" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Csi);
    EXPECT_EQ(events[0].finalByte, 'q');
    EXPECT_EQ(events[0].intermediate, ' ');
}

TEST_F(TestVt100Parser, can_aborts_in_progress_csi)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B[12" });
    parser.FeedByte(0x18); // CAN aborts
    parser.Feed(std::string_view{ "X" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Print);
    EXPECT_EQ(events[0].printChar, U'X');
}

TEST_F(TestVt100Parser, sub_aborts_in_progress_escape)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B" });
    parser.FeedByte(0x1A);
    parser.FeedByte('Y');

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Print);
    EXPECT_EQ(events[0].printChar, U'Y');
}

TEST_F(TestVt100Parser, reset_aborts_in_progress_sequence)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B[12" });
    parser.Reset();
    parser.FeedByte('Z');

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Print);
    EXPECT_EQ(events[0].printChar, U'Z');
}

TEST_F(TestVt100Parser, esc_in_csi_restarts_sequence)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B[12" });
    parser.Feed(std::string_view{ "\x1B"
                                  "D" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Esc);
    EXPECT_EQ(events[0].finalByte, 'D');
}

TEST_F(TestVt100Parser, byte_chunked_csi_is_recovered_correctly)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    const std::string seq = "\x1B[1;2H";
    for (char c : seq)
        parser.FeedByte(static_cast<uint8_t>(c));

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Csi);
    EXPECT_EQ(events[0].finalByte, 'H');
    ASSERT_EQ(events[0].params.size(), 2u);
    EXPECT_EQ(events[0].params[0], 1);
    EXPECT_EQ(events[0].params[1], 2);
}

TEST_F(TestVt100Parser, embedded_c0_in_csi_executes_inline)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B[1\r;2H" });

    // Expect: CR executed, then full CSI dispatched.
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Execute);
    EXPECT_EQ(events[0].executeByte, 0x0D);
    EXPECT_EQ(events[1].kind, ParserEvent::Kind::Csi);
    ASSERT_EQ(events[1].params.size(), 2u);
    EXPECT_EQ(events[1].params[0], 1);
    EXPECT_EQ(events[1].params[1], 2);
}

TEST_F(TestVt100Parser, c0_and_del_in_csi_entry_do_not_abort_sequence)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());
    const uint8_t bytes[] = { 0x1B, '[', 0x19, 0x7F, 'H' };

    parser.Feed(bytes, sizeof(bytes));

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Execute);
    EXPECT_EQ(events[0].executeByte, 0x19);
    EXPECT_EQ(events[1].kind, ParserEvent::Kind::Csi);
    EXPECT_EQ(events[1].finalByte, 'H');
}

TEST_F(TestVt100Parser, csi_entry_intermediate_is_dispatched)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B[ q" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Csi);
    EXPECT_EQ(events[0].finalByte, 'q');
    EXPECT_EQ(events[0].intermediate, ' ');
}

TEST_F(TestVt100Parser, del_in_csi_param_does_not_abort_sequence)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());
    const uint8_t bytes[] = { 0x1B, '[', '1', 0x7F, ';', '2', 'H' };

    parser.Feed(bytes, sizeof(bytes));

    ASSERT_EQ(events.size(), 1u);
    ASSERT_EQ(events[0].params.size(), 2u);
    EXPECT_EQ(events[0].params[0], 1);
    EXPECT_EQ(events[0].params[1], 2);
}

TEST_F(TestVt100Parser, c0_del_and_repeated_intermediate_in_csi_intermediate_are_handled)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());
    const uint8_t bytes[] = { 0x1B, '[', '1', ' ', 0x1C, 0x7F, '!', 'q' };

    parser.Feed(bytes, sizeof(bytes));

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Execute);
    EXPECT_EQ(events[0].executeByte, 0x1C);
    EXPECT_EQ(events[1].kind, ParserEvent::Kind::Csi);
    EXPECT_EQ(events[1].finalByte, 'q');
    EXPECT_EQ(events[1].intermediate, '!');
}

TEST_F(TestVt100Parser, invalid_csi_param_byte_is_ignored_until_final)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());
    const uint8_t bytes[] = { 0x1B, '[', '1', 0x80, 'H', 'A' };

    parser.Feed(bytes, sizeof(bytes));

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Print);
    EXPECT_EQ(events[0].printChar, U'A');
}

TEST_F(TestVt100Parser, invalid_csi_intermediate_byte_is_ignored_until_final)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());
    const uint8_t bytes[] = { 0x1B, '[', ' ', 0x80, 'H', 'A' };

    parser.Feed(bytes, sizeof(bytes));

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Print);
    EXPECT_EQ(events[0].printChar, U'A');
}

TEST_F(TestVt100Parser, c0_and_del_inside_escape_are_handled_without_aborting_escape)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());
    const uint8_t bytes[] = { 0x1B, 0x0D, 0x7F, 'D' };

    parser.Feed(bytes, sizeof(bytes));

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Execute);
    EXPECT_EQ(events[0].executeByte, 0x0D);
    EXPECT_EQ(events[1].kind, ParserEvent::Kind::Esc);
    EXPECT_EQ(events[1].finalByte, 'D');
}

TEST_F(TestVt100Parser, invalid_csi_bytes_are_ignored_until_final)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());
    const uint8_t bytes[] = { 0x1B, '[', 0x80, 'A', 'B', 'C' };

    parser.Feed(bytes, sizeof(bytes));
    parser.FeedByte('Z');

    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Print);
    EXPECT_EQ(events[0].printChar, U'B');
    EXPECT_EQ(events[1].printChar, U'C');
    EXPECT_EQ(events[2].printChar, U'Z');
}

TEST_F(TestVt100Parser, osc_terminated_by_bel)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B]0;hello\x07" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Osc);
    EXPECT_EQ(events[0].osc, std::string{ "0;hello" });
}

TEST_F(TestVt100Parser, osc_terminated_by_string_terminator)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B]2;title\x1B\\" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Osc);
    EXPECT_EQ(events[0].osc, std::string{ "2;title" });
}

TEST_F(TestVt100Parser, osc_can_aborts_without_dispatch)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B]0;title" });
    parser.FeedByte(0x18);
    parser.FeedByte('A');

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Print);
    EXPECT_EQ(events[0].printChar, U'A');
}

TEST_F(TestVt100Parser, osc_escape_non_terminator_is_reinterpreted_as_escape_sequence)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1B]0;title\x1B"
                                  "D" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Esc);
    EXPECT_EQ(events[0].finalByte, 'D');
}

TEST_F(TestVt100Parser, dcs_string_is_consumed_until_bel)
{
    tool::terminal::Vt100Parser parser(MakeCallbacks());

    parser.Feed(std::string_view{ "\x1BPignored\x07" });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, ParserEvent::Kind::Osc);
    EXPECT_EQ(events[0].osc, "ignored");
}
