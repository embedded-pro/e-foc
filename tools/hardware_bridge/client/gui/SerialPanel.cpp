#include "tools/hardware_bridge/client/gui/SerialPanel.hpp"
#include <QVBoxLayout>

namespace tool
{
    SerialPanel::SerialPanel(QWidget* parent)
        : QGroupBox("Serial", parent)
    {
        auto* layout = new QVBoxLayout(this);

        outputView = new AnsiTerminalWidget(this);
        layout->addWidget(outputView, 1);

        connect(outputView, &AnsiTerminalWidget::DataToSend, this, &SerialPanel::SendRequested);
    }

    void SerialPanel::AppendText(const QByteArray& data)
    {
        outputView->AppendData(data);
    }

    void SerialPanel::SetConnected(bool connected)
    {
        outputView->SetInputEnabled(connected);
    }
}
