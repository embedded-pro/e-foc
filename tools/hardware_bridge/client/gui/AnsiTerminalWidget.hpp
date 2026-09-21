#pragma once

#include "ui/terminal/AnsiPalette.hpp"
#include "ui/terminal/Vt100Terminal.hpp"
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
        void AppendData(const QByteArray& bytes);
        void Clear();

    signals:
        void DataToSend(QByteArray data);

    protected:
        void keyPressEvent(QKeyEvent* event) override;

    private:
        void Render();
        QColor MapForeground(ui::terminal::Color color) const;
        QColor MapBackground(ui::terminal::Color color) const;

        ui::terminal::Vt100Terminal terminal{ 24, 100 };
        bool inputEnabled{ false };

        ui::terminal::AnsiPalette ansiPalette;
    };
}
