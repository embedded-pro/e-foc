#include "tools/can_commander/gui/ConnectionPanel.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace tool
{
    ConnectionPanel::ConnectionPanel(CanBusAdapter& adapter, QWidget* parent)
        : QGroupBox("Connection", parent)
        , adapter(adapter)
    {
        auto* layout = new QVBoxLayout(this);

        auto* interfaceRow = new QHBoxLayout();
        interfaceRow->addWidget(new QLabel("Interface:"));
        interfaceCombo = new QComboBox();
        interfaceCombo->setEditable(true);
        interfaceCombo->setInsertPolicy(QComboBox::NoInsert);
        interfaceCombo->setMinimumWidth(100);
        interfaceRow->addWidget(interfaceCombo, 1);
        refreshButton = new QPushButton("↻");
        refreshButton->setFixedWidth(28);
        refreshButton->setToolTip("Refresh interface list");
        interfaceRow->addWidget(refreshButton);
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

        connect(refreshButton, &QPushButton::clicked, this, &ConnectionPanel::RefreshInterfaces);
        connect(connectButton, &QPushButton::clicked, this, &ConnectionPanel::ConnectRequested);
        connect(disconnectButton, &QPushButton::clicked, this, &ConnectionPanel::DisconnectRequested);

        RefreshInterfaces();
    }

    void ConnectionPanel::RefreshInterfaces()
    {
        QString current = interfaceCombo->currentText();
        interfaceCombo->clear();

        for (const auto& iface : adapter.AvailableInterfaces())
            interfaceCombo->addItem(QString::fromStdString(iface));

        if (interfaceCombo->count() == 0)
            interfaceCombo->addItem("can0");

        // Restore previous selection if still present, otherwise keep first item
        int idx = interfaceCombo->findText(current);
        if (idx >= 0)
            interfaceCombo->setCurrentIndex(idx);
    }

    QString ConnectionPanel::InterfaceName() const
    {
        return interfaceCombo->currentText();
    }

    uint32_t ConnectionPanel::Bitrate() const
    {
        return bitrateCombo->currentData().toUInt();
    }

    void ConnectionPanel::OnConnectionChanged(bool connected)
    {
        connectButton->setEnabled(!connected);
        disconnectButton->setEnabled(connected);
        interfaceCombo->setEnabled(!connected);
        refreshButton->setEnabled(!connected);
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
