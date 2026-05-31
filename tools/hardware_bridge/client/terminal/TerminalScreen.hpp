#pragma once

#include "tools/hardware_bridge/client/terminal/TerminalTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace tool::terminal
{
    class TerminalScreen;

    class TerminalCursorOperations
    {
    public:
        explicit TerminalCursorOperations(TerminalScreen& screen);

        void Up(int n);
        void Down(int n);
        void Forward(int n);
        void Backward(int n);
        void MoveTo(int row, int col);
        void MoveToColumn(int col);
        void Save();
        void Restore();

    private:
        TerminalScreen& screen_;
    };

    class TerminalTabStops
    {
    public:
        explicit TerminalTabStops(TerminalScreen& screen);

        void SetHere();
        void ClearHere();
        void ClearAll();

    private:
        TerminalScreen& screen_;
    };

    // VT100/VT102-style screen buffer with cursor, scroll region, tab stops,
    // pending-wrap, and a scrollback history of lines that have scrolled off
    // the top of the active screen.
    class TerminalScreen
    {
    public:
        explicit TerminalScreen(int rows = 24, int cols = 100);

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
        TerminalTabStops TabStops();

        // Cursor movement
        TerminalCursorOperations CursorOperations();

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
        friend class TerminalCursorOperations;
        friend class TerminalTabStops;

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
        std::vector<uint8_t> tabStops_;
        int scrollTop_{ 0 };
        int scrollBottom_{ 0 };
        bool pendingWrap_{ false };
        Modes modes_{};
        std::deque<std::vector<Cell>> history_;
        std::size_t maxHistory_{ 1000 };
    };
}
