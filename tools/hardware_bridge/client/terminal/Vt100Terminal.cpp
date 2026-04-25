#include "tools/hardware_bridge/client/terminal/Vt100Terminal.hpp"
#include <array>
#include <string>
#include <utility>

namespace tool::terminal
{
    namespace
    {
        int Param(const std::vector<int>& params, std::size_t index, int defaultValue)
        {
            if (index >= params.size())
                return defaultValue;
            int v = params[index];
            return v == 0 ? defaultValue : v;
        }

        int ParamRaw(const std::vector<int>& params, std::size_t index, int defaultValue)
        {
            if (index >= params.size())
                return defaultValue;
            return params[index];
        }

        struct ColorCode
        {
            int code;
            Color color;
        };

        using enum Color;

        constexpr std::array<ColorCode, 17> foregroundCodes{ {
            { 30, Black },
            { 31, Red },
            { 32, Green },
            { 33, Yellow },
            { 34, Blue },
            { 35, Magenta },
            { 36, Cyan },
            { 37, White },
            { 39, Default },
            { 90, BrightBlack },
            { 91, BrightRed },
            { 92, BrightGreen },
            { 93, BrightYellow },
            { 94, BrightBlue },
            { 95, BrightMagenta },
            { 96, BrightCyan },
            { 97, BrightWhite },
        } };

        constexpr std::array<ColorCode, 17> backgroundCodes{ {
            { 40, Black },
            { 41, Red },
            { 42, Green },
            { 43, Yellow },
            { 44, Blue },
            { 45, Magenta },
            { 46, Cyan },
            { 47, White },
            { 49, Default },
            { 100, BrightBlack },
            { 101, BrightRed },
            { 102, BrightGreen },
            { 103, BrightYellow },
            { 104, BrightBlue },
            { 105, BrightMagenta },
            { 106, BrightCyan },
            { 107, BrightWhite },
        } };

        bool ApplyColorCode(int parameter, std::span<const ColorCode> codes, Color& target)
        {
            for (const ColorCode& code : codes)
            {
                if (parameter == code.code)
                {
                    target = code.color;
                    return true;
                }
            }
            return false;
        }

        bool ApplyStyleCode(int parameter, Rendition& rendition)
        {
            if (parameter == 0)
                rendition = {};
            else if (parameter == 1)
                rendition.bold = true;
            else if (parameter == 2)
                rendition.faint = true;
            else if (parameter == 3)
                rendition.italic = true;
            else if (parameter == 4)
                rendition.underline = true;
            else if (parameter == 5)
                rendition.blink = true;
            else if (parameter == 7)
                rendition.inverse = true;
            else if (parameter == 22)
            {
                rendition.bold = false;
                rendition.faint = false;
            }
            else if (parameter == 23)
                rendition.italic = false;
            else if (parameter == 24)
                rendition.underline = false;
            else if (parameter == 25)
                rendition.blink = false;
            else if (parameter == 27)
                rendition.inverse = false;
            else
                return false;

            return true;
        }

        void ApplySgrParameter(int parameter, Rendition& rendition)
        {
            if (ApplyStyleCode(parameter, rendition))
                return;
            if (ApplyColorCode(parameter, foregroundCodes, rendition.foreground))
                return;
            ApplyColorCode(parameter, backgroundCodes, rendition.background);
        }

        void FillScreenWithAlignmentPattern(TerminalScreen& screen)
        {
            for (int row = 0; row < screen.Rows(); ++row)
            {
                for (int column = 0; column < screen.Cols(); ++column)
                {
                    screen.CursorOperations().MoveTo(row + 1, column + 1);
                    screen.Write(U'E');
                }
            }
            screen.CursorOperations().MoveTo(1, 1);
        }

        void ApplyPrivateMode(TerminalScreen& screen, int parameter, bool set)
        {
            if (parameter == 1)
                screen.GetModes().applicationCursorKeys = set;
            else if (parameter == 6)
                screen.GetModes().originMode = set;
            else if (parameter == 7)
                screen.GetModes().autoWrap = set;
            else if (parameter == 25)
                screen.GetModes().cursorVisible = set;
        }

        void ApplyAnsiMode(TerminalScreen& screen, int parameter, bool set)
        {
            if (parameter == 20)
                screen.GetModes().lineFeedNewLine = set;
        }
    }

    Vt100Terminal::Vt100Terminal(int rows, int cols)
        : screen_(rows, cols)
        , parser_(ParserCallbacks{
              [this](char32_t ch)
              {
                  OnPrint(ch);
              },
              [this](uint8_t b)
              {
                  OnExecute(b);
              },
              [this](char finalByte, char intermediate)
              {
                  OnEsc(finalByte, intermediate);
              },
              [this](char finalByte, const std::vector<int>& params, bool privateMarker, char intermediate)
              {
                  OnCsi(finalByte, params, privateMarker, intermediate);
              },
              [this](const std::string& payload)
              {
                  OnOsc(payload);
              },
          })
    {
    }

    void Vt100Terminal::Feed(std::span<const uint8_t> data)
    {
        parser_.Feed(data);
    }

    void Vt100Terminal::Feed(std::string_view data)
    {
        parser_.Feed(data);
    }

    void Vt100Terminal::FeedByte(uint8_t b)
    {
        parser_.FeedByte(b);
    }

    const TerminalScreen& Vt100Terminal::Screen() const
    {
        return screen_;
    }

    TerminalScreen& Vt100Terminal::Screen()
    {
        return screen_;
    }

    std::string Vt100Terminal::TakeOutgoing()
    {
        std::string out = std::move(outgoing_);
        outgoing_.clear();
        return out;
    }

    void Vt100Terminal::SetDeviceAttributesResponse(std::string response)
    {
        deviceAttributesResponse_ = std::move(response);
    }

    void Vt100Terminal::OnPrint(char32_t ch)
    {
        screen_.Write(ch);
    }

    void Vt100Terminal::OnExecute(uint8_t b)
    {
        switch (b)
        {
            case 0x08:
                screen_.Backspace();
                break; // BS
            case 0x09:
                screen_.HorizontalTab();
                break; // HT
            case 0x0A: // LF
            case 0x0B: // VT (treated as LF)
            case 0x0C:
                screen_.LineFeed();
                break; // FF (treated as LF)
            case 0x0D:
                screen_.CarriageReturn();
                break; // CR
            case 0x07:
                break; // BEL: silent
            case 0x00:
                break; // NUL: ignored
            case 0x05:
                break; // ENQ: no answerback configured
            case 0x11:
                break; // XON: ignored
            case 0x13:
                break; // XOFF: ignored
            default:
                break;
        }
    }

    void Vt100Terminal::OnEsc(char finalByte, char intermediate)
    {
        if (intermediate == '(' || intermediate == ')' || intermediate == '*' || intermediate == '+')
        {
            // Character set designation; ignored beyond accepting it.
            return;
        }
        if (intermediate == '#')
        {
            if (finalByte == '8')
                FillScreenWithAlignmentPattern(screen_);
            return;
        }
        if (intermediate != 0)
            return;

        switch (finalByte)
        {
            case 'D':
                screen_.Index();
                break; // IND
            case 'E':
                screen_.NextLine();
                break; // NEL
            case 'M':
                screen_.ReverseIndex();
                break; // RI
            case 'H':
                screen_.TabStops().SetHere();
                break; // HTS
            case '7':
                screen_.CursorOperations().Save();
                break; // DECSC
            case '8':
                screen_.CursorOperations().Restore();
                break; // DECRC
            case 'c':
                screen_.Reset();
                break; // RIS
            case '=':
                screen_.GetModes().applicationKeypad = true;
                break; // DECKPAM
            case '>':
                screen_.GetModes().applicationKeypad = false;
                break; // DECKPNM
            case 'Z':
                outgoing_ += deviceAttributesResponse_;
                break; // DECID
            default:
                break;
        }
    }

    void Vt100Terminal::OnCsi(char finalByte, const std::vector<int>& params, bool privateMarker, char intermediate)
    {
        if (intermediate != 0)
            return; // VT100 core does not implement intermediate-CSIs.

        switch (finalByte)
        {
            case 'A':
                screen_.CursorOperations().Up(Param(params, 0, 1));
                break;
            case 'B':
                screen_.CursorOperations().Down(Param(params, 0, 1));
                break;
            case 'C':
                screen_.CursorOperations().Forward(Param(params, 0, 1));
                break;
            case 'D':
                screen_.CursorOperations().Backward(Param(params, 0, 1));
                break;
            case 'E':
                screen_.CursorOperations().Down(Param(params, 0, 1));
                screen_.CursorOperations().MoveToColumn(1);
                break;
            case 'F':
                screen_.CursorOperations().Up(Param(params, 0, 1));
                screen_.CursorOperations().MoveToColumn(1);
                break;
            case 'G':
                screen_.CursorOperations().MoveToColumn(Param(params, 0, 1));
                break;
            case 'H':
            case 'f':
                screen_.CursorOperations().MoveTo(Param(params, 0, 1), Param(params, 1, 1));
                break;
            case 'J':
                screen_.EraseInDisplay(ParamRaw(params, 0, 0));
                break;
            case 'K':
                screen_.EraseInLine(ParamRaw(params, 0, 0));
                break;
            case 'g':
                if (ParamRaw(params, 0, 0) == 3)
                    screen_.TabStops().ClearAll();
                else
                    screen_.TabStops().ClearHere();
                break;
            case 'h':
                ApplyMode(params, true, privateMarker);
                break;
            case 'l':
                ApplyMode(params, false, privateMarker);
                break;
            case 'm':
                ApplySgr(params);
                break;
            case 'n': // DSR
                if (ParamRaw(params, 0, 0) == 5)
                    ReportDeviceStatus();
                else if (ParamRaw(params, 0, 0) == 6)
                    ReportCursorPosition();
                break;
            case 'c': // DA
                if (!privateMarker)
                    ReportDeviceAttributes();
                break;
            case 'r':
                screen_.SetScrollRegion(ParamRaw(params, 0, 1), ParamRaw(params, 1, screen_.Rows()));
                break;
            case 's':
                screen_.CursorOperations().Save();
                break; // SCO save cursor
            case 'u':
                screen_.CursorOperations().Restore();
                break; // SCO restore cursor
            default:
                break;
        }
    }

    void Vt100Terminal::OnOsc(const std::string& /*payload*/)
    {
        // OSC accepted and ignored. Title-setting could be exposed later.
    }

    void Vt100Terminal::ApplySgr(const std::vector<int>& params)
    {
        Rendition r = screen_.CurrentRendition();
        if (params.empty())
        {
            r = {};
            screen_.SetRendition(r);
            return;
        }

        for (int parameter : params)
            ApplySgrParameter(parameter, r);
        screen_.SetRendition(r);
    }

    void Vt100Terminal::ApplyMode(const std::vector<int>& params, bool set, bool privateMarker)
    {
        for (int p : params)
        {
            if (privateMarker)
                ApplyPrivateMode(screen_, p, set);
            else
                ApplyAnsiMode(screen_, p, set);
        }
    }

    void Vt100Terminal::ReportCursorPosition()
    {
        // 1-based row;column.
        const auto& c = screen_.Cursor();
        outgoing_ += "\x1B[";
        outgoing_ += std::to_string(c.row + 1);
        outgoing_ += ";";
        outgoing_ += std::to_string(c.column + 1);
        outgoing_ += "R";
    }

    void Vt100Terminal::ReportDeviceStatus()
    {
        outgoing_ += "\x1B[0n";
    }

    void Vt100Terminal::ReportDeviceAttributes()
    {
        outgoing_ += deviceAttributesResponse_;
    }
}
