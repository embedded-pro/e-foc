#include "source/tool/can_commander/gui/ConnectionPanel.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace tool
{
    ConnectionPanel::ConnectionPanel(QWidget* parent)
        : QGroupBox("Connection", parent)
    {
        auto* layout = new QVBoxLayout(this);

        auto* interfaceRow = new QHBoxLayout();
        interfaceRow->addWidget(new QLabel("Interface:"));
        interfaceEdit = new QLineEdit("can0");
        interfaceRow->addWidget(interfaceEdit);
        layout->addLayout(interfaceRow);

        auto* bitrateRow = new QHBoxLayout();
        bitrateRow->addWidget(new QLabel("Bitrate:"));
        bitrateCombo = new QComboBox();
        bitrateCombo->addItem("125 kbit/s", 125000);
        bitrateCombo->addItem("250 kbit/s", 250000);
        bitrateCombo->addItem("500 kbit/s", 500000);
        bitrateCombo->addItem("1 Mbit/s", 1000000);
        bitrateCombo->setCurrentIndex(3);
        bitrateRow->addWidget(bitrateCombo);
        layout->addLayout(bitrateRow);

        auto* buttonRow = new QHBoxLayout();
        connectButton = new QPushButton("Connect");
        disconnectButton = new QPushButton("Disconnect");
        disconnectButton->setEnabled(false);
        buttonRow->addWidget(connectButton);
        buttonRow->addWidget(disconnectButton);
        layout->addLayout(buttonRow);

        statusLabel = new QLabel("Disconnected");
        statusLabel->setStyleSheet("color: red; font-weight: bold;");
        layout->addWidget(statusLabel);

        connect(connectButton, &QPushButton::clicked, this, &ConnectionPanel::ConnectRequested);
        connect(disconnectButton, &QPushButton::clicked, this, &ConnectionPanel::DisconnectRequested);
    }

    QString ConnectionPanel::InterfaceName() const
    {
        return interfaceEdit->text();
    }

    uint32_t ConnectionPanel::Bitrate() const
    {
        return bitrateCombo->currentData().toUInt();
    }

    void ConnectionPanel::OnConnectionChanged(bool connected)
    {
        connectButton->setEnabled(!connected);
        disconnectButton->setEnabled(connected);
        interfaceEdit->setEnabled(!connected);
        bitrateCombo->setEnabled(!connected);

        if (connected)
        {
            statusLabel->setText("Connected");
            statusLabel->setStyleSheet("color: green; font-weight: bold;");
        }
        else
        {
            statusLabel->setText("Disconnected");
            statusLabel->setStyleSheet("color: red; font-weight: bold;");
        }
    }
}
