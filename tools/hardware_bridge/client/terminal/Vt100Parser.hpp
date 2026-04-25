#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace tool::terminal
{
    // Callbacks invoked by the byte-level VT100/ANSI parser.
    // The parser does not interpret semantics; it only classifies bytes
    // into print/execute/escape/CSI/OSC events.
    struct ParserCallbacks
    {
        // Printable character (>= 0x20, excluding 0x7F).
        std::function<void(char32_t)> Print;
        // C0 control character (< 0x20) including CR, LF, BS, HT, BEL, etc.
        // Excludes ESC, CAN, SUB, which are handled internally for state
        // transitions.
        std::function<void(uint8_t)> Execute;
        // ESC <intermediate?> <final>. For sequences like ESC D, ESC E,
        // ESC ( B, ESC # 8, ESC 7 / ESC 8, ESC c, ESC =, ESC >.
        std::function<void(char final, char intermediate)> EscDispatch;
        // CSI (ESC [) <private?> <params> <intermediate?> <final>.
        std::function<void(char final, const std::vector<int>& params, bool privateMarker, char intermediate)> CsiDispatch;
        // OSC (ESC ] ... ST/BEL): ignored payload-string consumed.
        std::function<void(const std::string& payload)> OscDispatch;
    };

    class Vt100Parser
    {
    public:
        explicit Vt100Parser(ParserCallbacks callbacks);

        void Feed(const uint8_t* data, std::size_t size);
        void Feed(std::string_view data);
        void FeedByte(uint8_t b);

        void Reset();

    private:
        enum class State : uint8_t
        {
            Ground,
            Escape,
            CsiEntry,
            CsiParam,
            CsiIntermediate,
            OscString,
            OscStringEsc, // saw ESC inside OSC, expecting backslash to terminate (ST)
            Ignore,       // consume bytes until a final terminates the sequence
        };

        void Transition(State next);
        void Anywhere(uint8_t b, bool& consumed);
        void HandleGround(uint8_t b);
        void HandleEscape(uint8_t b);
        void HandleCsiEntry(uint8_t b);
        void HandleCsiParam(uint8_t b);
        void HandleCsiIntermediate(uint8_t b);
        void HandleOsc(uint8_t b);
        void HandleOscEsc(uint8_t b);
        void DispatchCsi(char finalByte);

        ParserCallbacks callbacks_;
        State state_{ State::Ground };
        std::string params_;
        std::string oscBuffer_;
        char intermediate_{ 0 };
        bool privateMarker_{ false };
    };
}
