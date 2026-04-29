#include "tools/hardware_bridge/client/terminal/TerminalScreen.hpp"
#include <algorithm>

namespace tool::terminal
{
    namespace
    {
        constexpr int defaultTabInterval = 8;
    }

    TerminalScreen::TerminalScreen(int rows, int cols)
        : rows_(rows > 0 ? rows : 24)
        , cols_(cols > 0 ? cols : 100)
    {
        Reset();
    }

    int TerminalScreen::Rows() const
    {
        return rows_;
    }

    int TerminalScreen::Cols() const
    {
        return cols_;
    }

    void TerminalScreen::Reset()
    {
        currentRendition_ = {};
        savedRendition_ = {};
        cursor_ = {};
        savedCursor_ = {};
        modes_ = {};
        scrollTop_ = 0;
        scrollBottom_ = rows_ - 1;
        pendingWrap_ = false;

        grid_.assign(rows_, MakeBlankRow());
        tabStops_.assign(cols_, false);
        for (int c = defaultTabInterval; c < cols_; c += defaultTabInterval)
            tabStops_[c] = true;

        history_.clear();
    }

    void TerminalScreen::SoftReset()
    {
        currentRendition_ = {};
        savedRendition_ = {};
        modes_ = {};
        scrollTop_ = 0;
        scrollBottom_ = rows_ - 1;
        pendingWrap_ = false;
        savedCursor_ = {};
    }

    const Cell& TerminalScreen::At(int row, int col) const
    {
        return grid_[row][col];
    }

    const CursorPosition& TerminalScreen::Cursor() const
    {
        return cursor_;
    }

    const Rendition& TerminalScreen::CurrentRendition() const
    {
        return currentRendition_;
    }

    void TerminalScreen::SetRendition(const Rendition& rendition)
    {
        currentRendition_ = rendition;
    }

    const Modes& TerminalScreen::GetModes() const
    {
        return modes_;
    }

    Modes& TerminalScreen::GetModes()
    {
        return modes_;
    }

    const std::deque<std::vector<Cell>>& TerminalScreen::History() const
    {
        return history_;
    }

    void TerminalScreen::ClearHistory()
    {
        history_.clear();
    }

    Cell TerminalScreen::MakeBlankCell() const
    {
        return Cell{ U' ', currentRendition_ };
    }

    std::vector<Cell> TerminalScreen::MakeBlankRow() const
    {
        return std::vector<Cell>(static_cast<std::size_t>(cols_), MakeBlankCell());
    }

    void TerminalScreen::ClampCursor()
    {
        if (cursor_.row < 0)
            cursor_.row = 0;
        if (cursor_.row >= rows_)
            cursor_.row = rows_ - 1;
        if (cursor_.column < 0)
            cursor_.column = 0;
        if (cursor_.column >= cols_)
            cursor_.column = cols_ - 1;
    }

    void TerminalScreen::Write(char32_t ch)
    {
        if (pendingWrap_ && modes_.autoWrap)
        {
            CarriageReturn();
            Index();
        }
        pendingWrap_ = false;

        grid_[cursor_.row][cursor_.column] = Cell{ ch, currentRendition_ };

        if (cursor_.column + 1 >= cols_)
        {
            // Stay at last column; defer wrap until next printable.
            pendingWrap_ = modes_.autoWrap;
            cursor_.column = cols_ - 1;
        }
        else
        {
            ++cursor_.column;
        }
    }

    void TerminalScreen::CarriageReturn()
    {
        cursor_.column = 0;
        pendingWrap_ = false;
    }

    void TerminalScreen::LineFeed()
    {
        Index();
        if (modes_.lineFeedNewLine)
            CarriageReturn();
    }

    void TerminalScreen::Backspace()
    {
        if (cursor_.column > 0)
            --cursor_.column;
        pendingWrap_ = false;
    }

    void TerminalScreen::HorizontalTab()
    {
        pendingWrap_ = false;
        for (int c = cursor_.column + 1; c < cols_; ++c)
        {
            if (tabStops_[c])
            {
                cursor_.column = c;
                return;
            }
        }
        cursor_.column = cols_ - 1;
    }

    void TerminalScreen::Index()
    {
        pendingWrap_ = false;
        if (cursor_.row == scrollBottom_)
            ScrollUpInRegion(1);
        else if (cursor_.row < rows_ - 1)
            ++cursor_.row;
    }

    void TerminalScreen::NextLine()
    {
        Index();
        CarriageReturn();
    }

    void TerminalScreen::ReverseIndex()
    {
        pendingWrap_ = false;
        if (cursor_.row == scrollTop_)
            ScrollDownInRegion(1);
        else if (cursor_.row > 0)
            --cursor_.row;
    }

    TerminalTabStops TerminalScreen::TabStops()
    {
        return TerminalTabStops{ *this };
    }

    TerminalCursorOperations TerminalScreen::CursorOperations()
    {
        return TerminalCursorOperations{ *this };
    }

    TerminalTabStops::TerminalTabStops(TerminalScreen& screen)
        : screen_(screen)
    {
    }

    void TerminalTabStops::SetHere()
    {
        if (screen_.cursor_.column >= 0 && screen_.cursor_.column < screen_.cols_)
            screen_.tabStops_[screen_.cursor_.column] = true;
    }

    void TerminalTabStops::ClearHere()
    {
        if (screen_.cursor_.column >= 0 && screen_.cursor_.column < screen_.cols_)
            screen_.tabStops_[screen_.cursor_.column] = false;
    }

    void TerminalTabStops::ClearAll()
    {
        std::ranges::fill(screen_.tabStops_, false);
    }

    TerminalCursorOperations::TerminalCursorOperations(TerminalScreen& screen)
        : screen_(screen)
    {
    }

    void TerminalCursorOperations::Up(int n)
    {
        screen_.pendingWrap_ = false;
        if (n < 1)
            n = 1;
        screen_.cursor_.row = std::max(screen_.scrollTop_, screen_.cursor_.row - n);
    }

    void TerminalCursorOperations::Down(int n)
    {
        screen_.pendingWrap_ = false;
        if (n < 1)
            n = 1;
        screen_.cursor_.row = std::min(screen_.scrollBottom_, screen_.cursor_.row + n);
    }

    void TerminalCursorOperations::Forward(int n)
    {
        screen_.pendingWrap_ = false;
        if (n < 1)
            n = 1;
        screen_.cursor_.column = std::min(screen_.cols_ - 1, screen_.cursor_.column + n);
    }

    void TerminalCursorOperations::Backward(int n)
    {
        screen_.pendingWrap_ = false;
        if (n < 1)
            n = 1;
        screen_.cursor_.column = std::max(0, screen_.cursor_.column - n);
    }

    void TerminalCursorOperations::MoveTo(int row, int col)
    {
        screen_.pendingWrap_ = false;
        if (row < 1)
            row = 1;
        if (col < 1)
            col = 1;
        screen_.cursor_.row = row - 1;
        screen_.cursor_.column = col - 1;
        screen_.ClampCursor();
    }

    void TerminalCursorOperations::MoveToColumn(int col)
    {
        screen_.pendingWrap_ = false;
        if (col < 1)
            col = 1;
        screen_.cursor_.column = std::min(screen_.cols_ - 1, col - 1);
    }

    void TerminalCursorOperations::Save()
    {
        screen_.savedCursor_ = screen_.cursor_;
        screen_.savedRendition_ = screen_.currentRendition_;
    }

    void TerminalCursorOperations::Restore()
    {
        screen_.cursor_ = screen_.savedCursor_;
        screen_.currentRendition_ = screen_.savedRendition_;
        screen_.pendingWrap_ = false;
        screen_.ClampCursor();
    }

    void TerminalScreen::EraseInDisplay(int mode)
    {
        pendingWrap_ = false;
        const Cell blank = MakeBlankCell();
        auto eraseRow = [&](int row, int colFrom, int colTo)
        {
            for (int c = colFrom; c <= colTo; ++c)
                grid_[row][c] = blank;
        };

        if (mode == 0)
        {
            eraseRow(cursor_.row, cursor_.column, cols_ - 1);
            for (int r = cursor_.row + 1; r < rows_; ++r)
                eraseRow(r, 0, cols_ - 1);
        }
        else if (mode == 1)
        {
            for (int r = 0; r < cursor_.row; ++r)
                eraseRow(r, 0, cols_ - 1);
            eraseRow(cursor_.row, 0, cursor_.column);
        }
        else if (mode == 2 || mode == 3)
        {
            for (int r = 0; r < rows_; ++r)
                eraseRow(r, 0, cols_ - 1);
            if (mode == 3)
                history_.clear();
        }
    }

    void TerminalScreen::EraseInLine(int mode)
    {
        pendingWrap_ = false;
        const Cell blank = MakeBlankCell();
        if (mode == 0)
        {
            for (int c = cursor_.column; c < cols_; ++c)
                grid_[cursor_.row][c] = blank;
        }
        else if (mode == 1)
        {
            for (int c = 0; c <= cursor_.column; ++c)
                grid_[cursor_.row][c] = blank;
        }
        else if (mode == 2)
        {
            for (int c = 0; c < cols_; ++c)
                grid_[cursor_.row][c] = blank;
        }
    }

    void TerminalScreen::SetScrollRegion(int top, int bottom)
    {
        if (top < 1)
            top = 1;
        if (bottom < 1 || bottom > rows_)
            bottom = rows_;
        if (top >= bottom)
        {
            top = 1;
            bottom = rows_;
        }
        scrollTop_ = top - 1;
        scrollBottom_ = bottom - 1;
        cursor_.row = scrollTop_;
        cursor_.column = 0;
        pendingWrap_ = false;
    }

    int TerminalScreen::ScrollTop() const
    {
        return scrollTop_;
    }

    int TerminalScreen::ScrollBottom() const
    {
        return scrollBottom_;
    }

    void TerminalScreen::ScrollUpInRegion(int n)
    {
        if (n < 1)
            n = 1;
        const int regionRows = scrollBottom_ - scrollTop_ + 1;
        if (n > regionRows)
            n = regionRows;

        // Push displaced rows from top of region into history,
        // but only when the scroll region equals the full screen.
        const bool fullScreen = scrollTop_ == 0 && scrollBottom_ == rows_ - 1;
        for (int i = 0; i < n; ++i)
        {
            if (fullScreen)
            {
                history_.push_back(grid_[scrollTop_]);
                while (history_.size() > maxHistory_)
                    history_.pop_front();
            }

            for (int r = scrollTop_; r < scrollBottom_; ++r)
                grid_[r] = std::move(grid_[r + 1]);
            grid_[scrollBottom_] = MakeBlankRow();
        }
    }

    void TerminalScreen::ScrollDownInRegion(int n)
    {
        if (n < 1)
            n = 1;
        const int regionRows = scrollBottom_ - scrollTop_ + 1;
        if (n > regionRows)
            n = regionRows;

        for (int i = 0; i < n; ++i)
        {
            for (int r = scrollBottom_; r > scrollTop_; --r)
                grid_[r] = std::move(grid_[r - 1]);
            grid_[scrollTop_] = MakeBlankRow();
        }
    }

    std::string TerminalScreen::LineText(int row) const
    {
        std::string out;
        out.reserve(static_cast<std::size_t>(cols_));
        for (int c = 0; c < cols_; ++c)
        {
            char32_t ch = grid_[row][c].codepoint;
            if (ch <= 0x7F)
                out.push_back(static_cast<char>(ch));
            else
                out.push_back('?');
        }
        // Trim trailing spaces for convenience.
        while (!out.empty() && out.back() == ' ')
            out.pop_back();
        return out;
    }
}
