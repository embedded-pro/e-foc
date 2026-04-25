#include "tools/hardware_bridge/client/terminal/Vt100Terminal.hpp"
#include <cstdio>
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

    void Vt100Terminal::Feed(const uint8_t* data, std::size_t size)
    {
        parser_.Feed(data, size);
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
            {
                // DECALN: fill screen with 'E'.
                for (int r = 0; r < screen_.Rows(); ++r)
                    for (int c = 0; c < screen_.Cols(); ++c)
                    {
                        screen_.MoveCursor(r + 1, c + 1);
                        screen_.Write(U'E');
                    }
                screen_.MoveCursor(1, 1);
            }
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
                screen_.SetTabStop();
                break; // HTS
            case '7':
                screen_.SaveCursor();
                break; // DECSC
            case '8':
                screen_.RestoreCursor();
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
                screen_.CursorUp(Param(params, 0, 1));
                break;
            case 'B':
                screen_.CursorDown(Param(params, 0, 1));
                break;
            case 'C':
                screen_.CursorForward(Param(params, 0, 1));
                break;
            case 'D':
                screen_.CursorBackward(Param(params, 0, 1));
                break;
            case 'E':
                screen_.CursorDown(Param(params, 0, 1));
                screen_.CursorColumn(1);
                break;
            case 'F':
                screen_.CursorUp(Param(params, 0, 1));
                screen_.CursorColumn(1);
                break;
            case 'G':
                screen_.CursorColumn(Param(params, 0, 1));
                break;
            case 'H':
            case 'f':
                screen_.MoveCursor(Param(params, 0, 1), Param(params, 1, 1));
                break;
            case 'J':
                screen_.EraseInDisplay(ParamRaw(params, 0, 0));
                break;
            case 'K':
                screen_.EraseInLine(ParamRaw(params, 0, 0));
                break;
            case 'g':
                if (ParamRaw(params, 0, 0) == 3)
                    screen_.ClearAllTabStops();
                else
                    screen_.ClearTabStopHere();
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
                screen_.SaveCursor();
                break; // SCO save cursor
            case 'u':
                screen_.RestoreCursor();
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

        for (std::size_t i = 0; i < params.size(); ++i)
        {
            int p = params[i];
            switch (p)
            {
                case 0:
                    r = {};
                    break;
                case 1:
                    r.bold = true;
                    break;
                case 2:
                    r.faint = true;
                    break;
                case 3:
                    r.italic = true;
                    break;
                case 4:
                    r.underline = true;
                    break;
                case 5:
                    r.blink = true;
                    break;
                case 7:
                    r.inverse = true;
                    break;
                case 22:
                    r.bold = false;
                    r.faint = false;
                    break;
                case 23:
                    r.italic = false;
                    break;
                case 24:
                    r.underline = false;
                    break;
                case 25:
                    r.blink = false;
                    break;
                case 27:
                    r.inverse = false;
                    break;
                case 30:
                    r.foreground = Color::Black;
                    break;
                case 31:
                    r.foreground = Color::Red;
                    break;
                case 32:
                    r.foreground = Color::Green;
                    break;
                case 33:
                    r.foreground = Color::Yellow;
                    break;
                case 34:
                    r.foreground = Color::Blue;
                    break;
                case 35:
                    r.foreground = Color::Magenta;
                    break;
                case 36:
                    r.foreground = Color::Cyan;
                    break;
                case 37:
                    r.foreground = Color::White;
                    break;
                case 39:
                    r.foreground = Color::Default;
                    break;
                case 40:
                    r.background = Color::Black;
                    break;
                case 41:
                    r.background = Color::Red;
                    break;
                case 42:
                    r.background = Color::Green;
                    break;
                case 43:
                    r.background = Color::Yellow;
                    break;
                case 44:
                    r.background = Color::Blue;
                    break;
                case 45:
                    r.background = Color::Magenta;
                    break;
                case 46:
                    r.background = Color::Cyan;
                    break;
                case 47:
                    r.background = Color::White;
                    break;
                case 49:
                    r.background = Color::Default;
                    break;
                case 90:
                    r.foreground = Color::BrightBlack;
                    break;
                case 91:
                    r.foreground = Color::BrightRed;
                    break;
                case 92:
                    r.foreground = Color::BrightGreen;
                    break;
                case 93:
                    r.foreground = Color::BrightYellow;
                    break;
                case 94:
                    r.foreground = Color::BrightBlue;
                    break;
                case 95:
                    r.foreground = Color::BrightMagenta;
                    break;
                case 96:
                    r.foreground = Color::BrightCyan;
                    break;
                case 97:
                    r.foreground = Color::BrightWhite;
                    break;
                case 100:
                    r.background = Color::BrightBlack;
                    break;
                case 101:
                    r.background = Color::BrightRed;
                    break;
                case 102:
                    r.background = Color::BrightGreen;
                    break;
                case 103:
                    r.background = Color::BrightYellow;
                    break;
                case 104:
                    r.background = Color::BrightBlue;
                    break;
                case 105:
                    r.background = Color::BrightMagenta;
                    break;
                case 106:
                    r.background = Color::BrightCyan;
                    break;
                case 107:
                    r.background = Color::BrightWhite;
                    break;
                default:
                    break; // unknown SGR codes are ignored
            }
        }
        screen_.SetRendition(r);
    }

    void Vt100Terminal::ApplyMode(const std::vector<int>& params, bool set, bool privateMarker)
    {
        for (int p : params)
        {
            if (privateMarker)
            {
                switch (p)
                {
                    case 1:
                        screen_.GetModes().applicationCursorKeys = set;
                        break; // DECCKM
                    case 6:
                        screen_.GetModes().originMode = set;
                        break; // DECOM
                    case 7:
                        screen_.GetModes().autoWrap = set;
                        break; // DECAWM
                    case 25:
                        screen_.GetModes().cursorVisible = set;
                        break; // DECTCEM
                    default:
                        break;
                }
            }
            else
            {
                switch (p)
                {
                    case 20:
                        screen_.GetModes().lineFeedNewLine = set;
                        break; // LNM
                    default:
                        break;
                }
            }
        }
    }

    void Vt100Terminal::ReportCursorPosition()
    {
        // 1-based row;column.
        const auto& c = screen_.Cursor();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "\x1B[%d;%dR", c.row + 1, c.column + 1);
        outgoing_ += buf;
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
