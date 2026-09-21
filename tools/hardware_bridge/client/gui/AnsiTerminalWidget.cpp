#include "tools/hardware_bridge/client/gui/AnsiTerminalWidget.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include <QFont>
#include <QPalette>
#include <QScrollBar>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <algorithm>
#include <span>

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
        p.setColor(QPalette::Base, MapBackground(ui::terminal::Color::Default));
        p.setColor(QPalette::Text, MapForeground(ui::terminal::Color::Default));
        setPalette(p);

        Render();
    }

    QColor AnsiTerminalWidget::MapForeground(ui::terminal::Color color) const
    {
        return ui::backend::qt::ToQt(ansiPalette.Foreground(color));
    }

    QColor AnsiTerminalWidget::MapBackground(ui::terminal::Color color) const
    {
        return ui::backend::qt::ToQt(ansiPalette.Background(color));
    }

    void AnsiTerminalWidget::AppendData(const QByteArray& bytes)
    {
        const bool atBottom = verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 1;

        terminal.Feed(std::span{
            reinterpret_cast<const uint8_t*>(bytes.constData()),
            static_cast<std::size_t>(bytes.size()) });

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

        auto lastContentColumn = [](const std::vector<ui::terminal::Cell>& row)
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

        std::vector<std::vector<ui::terminal::Cell>> rows;
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
            std::vector<ui::terminal::Cell> row;
            row.reserve(static_cast<std::size_t>(screen.Cols()));
            for (int c = 0; c < screen.Cols(); ++c)
                row.push_back(screen.At(r, c));
            rows.push_back(std::move(row));
        }

        struct Run
        {
            int start;
            int length;
            ui::terminal::Rendition rendition;
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

        setPlainText(text);

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
