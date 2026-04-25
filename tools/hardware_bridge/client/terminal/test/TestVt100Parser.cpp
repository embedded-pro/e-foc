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
