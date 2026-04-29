#pragma once

#include "tools/hardware_bridge/client/terminal/Vt100Terminal.hpp"
#include <QByteArray>
#include <QColor>
#include <QKeyEvent>
#include <QPlainTextEdit>

namespace tool
{
    class AnsiTerminalWidget
        : public QPlainTextEdit
    {
        Q_OBJECT

    public:
        explicit AnsiTerminalWidget(QWidget* parent = nullptr);

        void SetInputEnabled(bool enabled);

    public slots:
        void AppendData(const QByteArray& data);
        void Clear();

    signals:
        void DataToSend(QByteArray data);

    protected:
        void keyPressEvent(QKeyEvent* event) override;

    private:
        void Render();
        QColor MapForeground(terminal::Color color) const;
        QColor MapBackground(terminal::Color color) const;

        terminal::Vt100Terminal terminal{ 24, 100 };
        bool inputEnabled{ false };

        const QColor defaultFg{ 0xCC, 0xCC, 0xCC };
        const QColor defaultBg{ 0x1E, 0x1E, 0x1E };
    };
}
