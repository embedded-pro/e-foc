#include "tools/hardware_bridge/client/gui/AnsiTerminalWidget.hpp"
#include <QFont>
#include <QPalette>
#include <QScrollBar>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <algorithm>

namespace
{
    using tool::terminal::Color;

    // Dark-mode ANSI color palette (standard 8 colors).
    const QColor ansiColors[8] = {
        { 0x2D, 0x2D, 0x2D }, // Black
        { 0xE7, 0x4C, 0x3C }, // Red
        { 0x2E, 0xCC, 0x71 }, // Green
        { 0xF1, 0xC4, 0x0F }, // Yellow
        { 0x34, 0x98, 0xDB }, // Blue
        { 0x9B, 0x59, 0xB6 }, // Magenta
        { 0x1A, 0xBC, 0x9C }, // Cyan
        { 0xCC, 0xCC, 0xCC }, // White
    };

    // Bright variants (SGR 90-97 / 100-107).
    const QColor ansiBrightColors[8] = {
        { 0x80, 0x80, 0x80 }, // Bright Black
        { 0xFF, 0x79, 0x6B }, // Bright Red
        { 0x5F, 0xF9, 0x67 }, // Bright Green
        { 0xFF, 0xFF, 0x2B }, // Bright Yellow
        { 0x6B, 0xC5, 0xFF }, // Bright Blue
        { 0xFF, 0x92, 0xFF }, // Bright Magenta
        { 0x5F, 0xFF, 0xEA }, // Bright Cyan
        { 0xFF, 0xFF, 0xFF }, // Bright White
    };
}

namespace tool
{
    AnsiTerminalWidget::AnsiTerminalWidget(QWidget* parent)
        : QPlainTextEdit(parent)
    {
        setReadOnly(true);
        setFont(QFont("Monospace", 9));
        setLineWrapMode(QPlainTextEdit::NoWrap);
        setUndoRedoEnabled(false);
        setMaximumBlockCount(2000);

        QPalette p = palette();
        p.setColor(QPalette::Base, defaultBg);
        p.setColor(QPalette::Text, defaultFg);
        setPalette(p);

        Render();
    }

    QColor AnsiTerminalWidget::MapForeground(terminal::Color color) const
    {
        switch (color)
        {
            case Color::Default:
                return defaultFg;
            case Color::Black:
                return ansiColors[0];
            case Color::Red:
                return ansiColors[1];
            case Color::Green:
                return ansiColors[2];
            case Color::Yellow:
                return ansiColors[3];
            case Color::Blue:
                return ansiColors[4];
            case Color::Magenta:
                return ansiColors[5];
            case Color::Cyan:
                return ansiColors[6];
            case Color::White:
                return ansiColors[7];
            case Color::BrightBlack:
                return ansiBrightColors[0];
            case Color::BrightRed:
                return ansiBrightColors[1];
            case Color::BrightGreen:
                return ansiBrightColors[2];
            case Color::BrightYellow:
                return ansiBrightColors[3];
            case Color::BrightBlue:
                return ansiBrightColors[4];
            case Color::BrightMagenta:
                return ansiBrightColors[5];
            case Color::BrightCyan:
                return ansiBrightColors[6];
            case Color::BrightWhite:
                return ansiBrightColors[7];
        }
        return defaultFg;
    }

    QColor AnsiTerminalWidget::MapBackground(terminal::Color color) const
    {
        if (color == Color::Default)
            return defaultBg;
        return MapForeground(color);
    }

    void AnsiTerminalWidget::AppendData(const QByteArray& data)
    {
        const bool atBottom = verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 1;

        terminal.Feed(reinterpret_cast<const uint8_t*>(data.constData()), static_cast<std::size_t>(data.size()));

        std::string outgoing = terminal.TakeOutgoing();
        if (!outgoing.empty())
            emit DataToSend(QByteArray(outgoing.data(), static_cast<int>(outgoing.size())));

        Render();

        if (atBottom)
            verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }

    void AnsiTerminalWidget::Render()
    {
        const auto& screen = terminal.Screen();

        auto lastContentColumn = [](const std::vector<terminal::Cell>& row)
        {
            for (std::size_t i = row.size(); i > 0; --i)
                if (row[i - 1].codepoint != U' ')
                    return static_cast<int>(i);
            return 0;
        };

        auto isBlank = [&](int r)
        {
            for (int c = 0; c < screen.Cols(); ++c)
                if (screen.At(r, c).codepoint != U' ')
                    return false;
            return true;
        };

        // Collect rows: history first, then active screen up to the last
        // non-blank row (or the cursor row, whichever is lower) so trailing
        // empty rows aren't padded onto the document.
        std::vector<std::vector<terminal::Cell>> rows;
        rows.reserve(screen.History().size() + static_cast<std::size_t>(screen.Rows()));
        for (const auto& row : screen.History())
            rows.push_back(row);

        int lastRow = screen.Cursor().row;
        for (int r = screen.Rows() - 1; r >= 0; --r)
        {
            if (!isBlank(r))
            {
                lastRow = std::max(lastRow, r);
                break;
            }
        }
        for (int r = 0; r <= lastRow; ++r)
        {
            std::vector<terminal::Cell> row;
            row.reserve(static_cast<std::size_t>(screen.Cols()));
            for (int c = 0; c < screen.Cols(); ++c)
                row.push_back(screen.At(r, c));
            rows.push_back(std::move(row));
        }

        // Build the full plain text and a flat list of (start, length,
        // rendition) runs so we can apply char formats in a single pass
        // after setPlainText.
        struct Run
        {
            int start;
            int length;
            terminal::Rendition rendition;
        };
        QString text;
        std::vector<Run> runs;

        for (std::size_t rowIdx = 0; rowIdx < rows.size(); ++rowIdx)
        {
            if (rowIdx != 0)
                text.append(QChar('\n'));

            const auto& row = rows[rowIdx];
            const int lastCol = lastContentColumn(row);
            if (lastCol == 0)
                continue;

            std::size_t runStart = 0;
            while (runStart < static_cast<std::size_t>(lastCol))
            {
                std::size_t runEnd = runStart + 1;
                while (runEnd < static_cast<std::size_t>(lastCol) && row[runEnd].rendition == row[runStart].rendition)
                    ++runEnd;

                const int runLen = static_cast<int>(runEnd - runStart);
                const int runStartInDoc = text.size();
                for (std::size_t i = runStart; i < runEnd; ++i)
                {
                    char32_t cp = row[i].codepoint;
                    text.append(cp <= 0x7F ? QChar(static_cast<char>(cp)) : QChar(QChar::ReplacementCharacter));
                }

                runs.push_back({ runStartInDoc, runLen, row[runStart].rendition });
                runStart = runEnd;
            }
        }

        // Install content via the idiomatic API; QPlainTextEdit handles
        // block construction unambiguously here (no empty-block-0 trap).
        setPlainText(text);

        // Apply character formats per run.
        QTextCursor cursor(document());
        cursor.beginEditBlock();
        for (const Run& run : runs)
        {
            QTextCharFormat fmt;
            fmt.setForeground(MapForeground(run.rendition.inverse ? run.rendition.background : run.rendition.foreground));
            fmt.setBackground(MapBackground(run.rendition.inverse ? run.rendition.foreground : run.rendition.background));
            if (run.rendition.bold)
                fmt.setFontWeight(QFont::Bold);
            if (run.rendition.italic)
                fmt.setFontItalic(true);
            if (run.rendition.underline)
                fmt.setFontUnderline(true);

            cursor.setPosition(run.start);
            cursor.setPosition(run.start + run.length, QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(fmt);
        }
        cursor.endEditBlock();
    }

    void AnsiTerminalWidget::SetInputEnabled(bool enabled)
    {
        inputEnabled = enabled;
    }

    void AnsiTerminalWidget::keyPressEvent(QKeyEvent* event)
    {
        if (!inputEnabled)
        {
            event->accept();
            return;
        }

        QByteArray bytes;

        if (event->modifiers() & Qt::ControlModifier)
        {
            const int key = event->key();
            if (key >= Qt::Key_A && key <= Qt::Key_Z)
                bytes.append(static_cast<char>(key - Qt::Key_A + 1));
        }
        else
        {
            switch (event->key())
            {
                case Qt::Key_Return:
                case Qt::Key_Enter:
                    bytes = QByteArray("\r", 1);
                    break;
                case Qt::Key_Backspace:
                    bytes = QByteArray("\x7f", 1);
                    break;
                case Qt::Key_Tab:
                    bytes = QByteArray("\t", 1);
                    break;
                case Qt::Key_Up:
                    bytes = QByteArray("\x1b[A");
                    break;
                case Qt::Key_Down:
                    bytes = QByteArray("\x1b[B");
                    break;
                case Qt::Key_Right:
                    bytes = QByteArray("\x1b[C");
                    break;
                case Qt::Key_Left:
                    bytes = QByteArray("\x1b[D");
                    break;
                case Qt::Key_Home:
                    bytes = QByteArray("\x1b[H");
                    break;
                case Qt::Key_End:
                    bytes = QByteArray("\x1b[F");
                    break;
                case Qt::Key_Delete:
                    bytes = QByteArray("\x1b[3~");
                    break;
                default:
                    bytes = event->text().toUtf8();
                    break;
            }
        }

        event->accept();
        if (!bytes.isEmpty())
            emit DataToSend(bytes);
    }

    void AnsiTerminalWidget::Clear()
    {
        terminal.Screen().Reset();
        Render();
    }
}
