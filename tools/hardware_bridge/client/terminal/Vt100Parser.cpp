#include "tools/hardware_bridge/client/terminal/Vt100Parser.hpp"

namespace tool::terminal
{
    namespace
    {
        constexpr uint8_t ESC_BYTE = 0x1B;
        constexpr uint8_t CAN_BYTE = 0x18;
        constexpr uint8_t SUB_BYTE = 0x1A;
        constexpr uint8_t BEL_BYTE = 0x07;
        constexpr uint8_t DEL_BYTE = 0x7F;

        bool IsC0Execute(uint8_t b)
        {
            // 0x00..0x17, 0x19, 0x1C..0x1F. ESC, CAN, SUB are handled separately.
            if (b == ESC_BYTE || b == CAN_BYTE || b == SUB_BYTE)
                return false;
            if (b <= 0x17)
                return true;
            if (b == 0x19)
                return true;
            if (b >= 0x1C && b <= 0x1F)
                return true;
            return false;
        }

        bool IsIntermediate(uint8_t b)
        {
            return b >= 0x20 && b <= 0x2F;
        }

        bool IsParameter(uint8_t b)
        {
            return (b >= 0x30 && b <= 0x39) || b == 0x3B || b == 0x3A;
        }

        bool IsPrivateMarker(uint8_t b)
        {
            return b >= 0x3C && b <= 0x3F;
        }

        bool IsFinal(uint8_t b)
        {
            return b >= 0x40 && b <= 0x7E;
        }
    }

    Vt100Parser::Vt100Parser(ParserCallbacks callbacks)
        : callbacks_(std::move(callbacks))
    {
    }

    void Vt100Parser::Reset()
    {
        using enum State;

        state_ = Ground;
        params_.clear();
        oscBuffer_.clear();
        intermediate_ = 0;
        privateMarker_ = false;
    }

    void Vt100Parser::Transition(State next)
    {
        using enum State;

        state_ = next;
        if (next == CsiEntry)
        {
            params_.clear();
            intermediate_ = 0;
            privateMarker_ = false;
        }
        else if (next == Escape)
        {
            intermediate_ = 0;
        }
        else if (next == OscString)
        {
            oscBuffer_.clear();
        }
    }

    void Vt100Parser::Feed(std::span<const uint8_t> data)
    {
        for (uint8_t byte : data)
            FeedByte(byte);
    }

    void Vt100Parser::Feed(std::string_view data)
    {
        for (char c : data)
            FeedByte(static_cast<uint8_t>(c));
    }

    void Vt100Parser::Anywhere(uint8_t b, bool& consumed)
    {
        using enum State;

        consumed = true;
        if (b == ESC_BYTE)
        {
            // ESC always restarts; abandons any in-progress sequence.
            Transition(Escape);
        }
        else if (b == CAN_BYTE || b == SUB_BYTE)
        {
            // Abort current sequence; SUB conventionally also displays a
            // substitute character, but we silently drop it for now.
            Transition(Ground);
        }
        else
        {
            consumed = false;
        }
    }

    void Vt100Parser::FeedByte(uint8_t b)
    {
        using enum State;

        // OSC must capture ESC as the first half of ST (ESC \), so handle
        // OSC states before the generic anywhere transitions.
        if (state_ == OscString)
        {
            HandleOsc(b);
            return;
        }
        if (state_ == OscStringEsc)
        {
            HandleOscEsc(b);
            return;
        }

        bool consumed = false;
        Anywhere(b, consumed);
        if (consumed)
            return;

        switch (state_)
        {
            case Ground:
                HandleGround(b);
                break;
            case Escape:
                HandleEscape(b);
                break;
            case CsiEntry:
                HandleCsiEntry(b);
                break;
            case CsiParam:
                HandleCsiParam(b);
                break;
            case CsiIntermediate:
                HandleCsiIntermediate(b);
                break;
            case Ignore:
                if (IsFinal(b))
                    Transition(Ground);
                break;
            case OscString:
            case OscStringEsc:
                break; // handled above
        }
    }

    void Vt100Parser::HandleGround(uint8_t b) const
    {
        if (b == DEL_BYTE)
            return;
        if (IsC0Execute(b))
        {
            if (callbacks_.Execute)
                callbacks_.Execute(b);
            return;
        }
        if (b >= 0x20)
        {
            if (callbacks_.Print)
                callbacks_.Print(static_cast<char32_t>(b));
        }
    }

    void Vt100Parser::HandleEscape(uint8_t b)
    {
        using enum State;

        if (IsC0Execute(b))
        {
            if (callbacks_.Execute)
                callbacks_.Execute(b);
            return;
        }
        if (b == DEL_BYTE)
            return;
        if (IsIntermediate(b))
        {
            // Collect a single intermediate byte; subsequent intermediates
            // overwrite for simplicity (sufficient for VT100 sequences).
            intermediate_ = static_cast<char>(b);
            return;
        }
        if (b == '[')
        {
            Transition(CsiEntry);
            return;
        }
        if (b == ']')
        {
            Transition(OscString);
            return;
        }
        if (b == 'P' || b == 'X' || b == '^' || b == '_')
        {
            // DCS/SOS/PM/APC: consume the string until ST or BEL.
            Transition(OscString);
            return;
        }
        if (b == '\\')
        {
            // Lone ST outside string mode: ignore.
            Transition(Ground);
            return;
        }
        if (IsFinal(b) || (b >= 0x30 && b <= 0x3F))
        {
            // ESC <intermediate?> <final>
            if (callbacks_.EscDispatch)
                callbacks_.EscDispatch(static_cast<char>(b), intermediate_);
            Transition(Ground);
            return;
        }
    }

    void Vt100Parser::HandleCsiEntry(uint8_t b)
    {
        using enum State;

        if (IsC0Execute(b))
        {
            if (callbacks_.Execute)
                callbacks_.Execute(b);
            return;
        }
        if (b == DEL_BYTE)
            return;
        if (IsPrivateMarker(b))
        {
            privateMarker_ = b == '?';
            // Other private markers (=, >, <) are tolerated but not flagged.
            Transition(CsiParam);
            return;
        }
        if (IsParameter(b))
        {
            params_.push_back(static_cast<char>(b == 0x3A ? 0x3B : b));
            Transition(CsiParam);
            return;
        }
        if (IsIntermediate(b))
        {
            intermediate_ = static_cast<char>(b);
            Transition(CsiIntermediate);
            return;
        }
        if (IsFinal(b))
        {
            DispatchCsi(static_cast<char>(b));
            return;
        }
        Transition(Ignore);
    }

    void Vt100Parser::HandleCsiParam(uint8_t b)
    {
        using enum State;

        if (IsC0Execute(b))
        {
            if (callbacks_.Execute)
                callbacks_.Execute(b);
            return;
        }
        if (b == DEL_BYTE)
            return;
        if (IsParameter(b))
        {
            params_.push_back(static_cast<char>(b == 0x3A ? 0x3B : b));
            return;
        }
        if (IsIntermediate(b))
        {
            intermediate_ = static_cast<char>(b);
            Transition(CsiIntermediate);
            return;
        }
        if (IsFinal(b))
        {
            DispatchCsi(static_cast<char>(b));
            return;
        }
        Transition(Ignore);
    }

    void Vt100Parser::HandleCsiIntermediate(uint8_t b)
    {
        using enum State;

        if (IsC0Execute(b))
        {
            if (callbacks_.Execute)
                callbacks_.Execute(b);
            return;
        }
        if (b == DEL_BYTE)
            return;
        if (IsIntermediate(b))
        {
            intermediate_ = static_cast<char>(b);
            return;
        }
        if (IsFinal(b))
        {
            DispatchCsi(static_cast<char>(b));
            return;
        }
        Transition(Ignore);
    }

    void Vt100Parser::DispatchCsi(char finalByte)
    {
        using enum State;

        std::vector<int> values;
        int current = 0;
        bool hasDigit = false;
        for (char c : params_)
        {
            if (c >= '0' && c <= '9')
            {
                current = current * 10 + (c - '0');
                hasDigit = true;
            }
            else if (c == ';')
            {
                values.push_back(hasDigit ? current : 0);
                current = 0;
                hasDigit = false;
            }
        }
        if (hasDigit || !params_.empty())
            values.push_back(current);

        if (callbacks_.CsiDispatch)
            callbacks_.CsiDispatch(finalByte, values, privateMarker_, intermediate_);

        Transition(Ground);
    }

    void Vt100Parser::HandleOsc(uint8_t b)
    {
        using enum State;

        if (b == BEL_BYTE)
        {
            if (callbacks_.OscDispatch)
                callbacks_.OscDispatch(oscBuffer_);
            Transition(Ground);
            return;
        }
        if (b == ESC_BYTE)
        {
            state_ = OscStringEsc;
            return;
        }
        if (b == CAN_BYTE || b == SUB_BYTE)
        {
            Transition(Ground);
            return;
        }
        oscBuffer_.push_back(static_cast<char>(b));
    }

    void Vt100Parser::HandleOscEsc(uint8_t b)
    {
        using enum State;

        if (b == '\\')
        {
            if (callbacks_.OscDispatch)
                callbacks_.OscDispatch(oscBuffer_);
            Transition(Ground);
            return;
        }
        // Not a valid ST; abandon the OSC and reinterpret the byte from
        // the Escape state so the user's intent is not lost.
        Transition(Escape);
        FeedByte(b);
    }
}
