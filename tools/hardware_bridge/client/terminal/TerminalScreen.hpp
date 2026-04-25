#pragma once

#include "tools/hardware_bridge/client/terminal/TerminalTypes.hpp"
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

namespace tool::terminal
{
    // VT100/VT102-style screen buffer with cursor, scroll region, tab stops,
    // pending-wrap, and a scrollback history of lines that have scrolled off
    // the top of the active screen.
    class TerminalScreen
    {
    public:
        TerminalScreen(int rows = 24, int cols = 100);

        int Rows() const;
        int Cols() const;

        // Reset to power-on state (RIS). Clears history.
        void Reset();
        // Soft reset (DECSTR). Resets modes/rendition/scroll region but
        // does not clear screen or history.
        void SoftReset();

        const Cell& At(int row, int col) const;
        const CursorPosition& Cursor() const;
        const Rendition& CurrentRendition() const;
        void SetRendition(const Rendition& rendition);

        const Modes& GetModes() const;
        Modes& GetModes();

        // Scrollback (oldest at front, newest at back).
        const std::deque<std::vector<Cell>>& History() const;
        void ClearHistory();

        // Writing
        void Write(char32_t ch);

        // C0
        void CarriageReturn();
        void LineFeed();
        void Backspace();
        void HorizontalTab();

        // ESC single-character format effectors
        void Index();        // ESC D
        void NextLine();     // ESC E
        void ReverseIndex(); // ESC M

        // Tab stops
        void SetTabStop();       // HTS
        void ClearTabStopHere(); // TBC 0
        void ClearAllTabStops(); // TBC 3

        // Cursor movement
        void CursorUp(int n);
        void CursorDown(int n);
        void CursorForward(int n);
        void CursorBackward(int n);
        // 1-based addressing as used by CUP/HVP.
        void MoveCursor(int row, int col);
        void CursorColumn(int col); // 1-based, CHA

        // DECSC / DECRC
        void SaveCursor();
        void RestoreCursor();

        // Erase
        void EraseInDisplay(int mode);
        void EraseInLine(int mode);

        // Scroll region (1-based, inclusive). Clamped to screen.
        void SetScrollRegion(int top, int bottom);
        int ScrollTop() const;    // 0-based
        int ScrollBottom() const; // 0-based

        // Helpers (testing/rendering)
        std::string LineText(int row) const;

    private:
        void ScrollUpInRegion(int n);
        void ScrollDownInRegion(int n);
        std::vector<Cell> MakeBlankRow() const;
        Cell MakeBlankCell() const;
        void ClampCursor();

        int rows_{};
        int cols_{};
        std::vector<std::vector<Cell>> grid_;
        CursorPosition cursor_{};
        CursorPosition savedCursor_{};
        Rendition currentRendition_{};
        Rendition savedRendition_{};
        std::vector<bool> tabStops_;
        int scrollTop_{ 0 };
        int scrollBottom_{ 0 };
        bool pendingWrap_{ false };
        Modes modes_{};
        std::deque<std::vector<Cell>> history_;
        std::size_t maxHistory_{ 1000 };
    };
}
