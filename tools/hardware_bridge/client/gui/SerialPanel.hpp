#pragma once

#include "tools/hardware_bridge/client/gui/AnsiTerminalWidget.hpp"
#include <QByteArray>
#include <QGroupBox>

namespace tool
{
    class SerialPanel
        : public QGroupBox
    {
        Q_OBJECT

    public:
        explicit SerialPanel(QWidget* parent = nullptr);

        void AppendText(const QByteArray& data);
        void SetConnected(bool connected);

    signals:
        void SendRequested(QByteArray data);

    private:
        AnsiTerminalWidget* outputView;
    };
}
