#include "tools/can_commander/gui/ConnectionPanel.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QHBoxLayout>

namespace tool
{
    ConnectionPanel::ConnectionPanel(CanBusAdapter& adapter, QWidget* parent)
        : QGroupBox("Connection", parent)
        , adapter(adapter)
    {
        auto* layout = new QHBoxLayout(this);
        CreateFields(*layout);
        CreateButtons(*layout);

        statusLabel = new QLabel();
        layout->addWidget(statusLabel, 1);

        ConnectSignals();
        RefreshInterfaces();
        OnConnectionChanged(false);
    }

    void ConnectionPanel::AddLabelled(QHBoxLayout& layout, const char* text, QWidget* field)
    {
        auto* label = new QLabel(text);
        label->setBuddy(field);
        layout.addWidget(label);
        layout.addWidget(field);
    }

    void ConnectionPanel::CreateFields(QHBoxLayout& layout)
    {
        interfaceCombo = new QComboBox();
        interfaceCombo->setEditable(true);
        interfaceCombo->setInsertPolicy(QComboBox::NoInsert);
        interfaceCombo->setMinimumWidth(120);
        interfaceCombo->setToolTip("CAN interface to open, e.g. can0 or a PCAN/Kvaser/CANable channel");
        AddLabelled(layout, "&Interface:", interfaceCombo);

        refreshButton = new QPushButton("↻");
        refreshButton->setFixedWidth(28);
        refreshButton->setToolTip("Refresh interface list");
        refreshButton->setAccessibleName("Refresh interface list");
        layout.addWidget(refreshButton);

        bitrateCombo = new QComboBox();
        for (const auto bitrate : { 125000, 250000, 500000 })
            bitrateCombo->addItem(QString("%1 kbit/s").arg(bitrate / 1000), bitrate);
        bitrateCombo->addItem("1 Mbit/s", 1000000);
        bitrateCombo->setCurrentIndex(3);
        AddLabelled(layout, "&Bitrate:", bitrateCombo);

        nodeIdSpin = new QSpinBox();
        nodeIdSpin->setRange(1, 4095);
        nodeIdSpin->setValue(1);
        nodeIdSpin->setToolTip("CAN node ID of the motor controller to command");
        AddLabelled(layout, "&Node ID:", nodeIdSpin);
    }

    void ConnectionPanel::CreateButtons(QHBoxLayout& layout)
    {
        connectButton = new QPushButton("&Connect");
        ui::backend::qt::StyleButton(*connectButton, ui::theme::ButtonRole::Primary);
        disconnectButton = new QPushButton("&Disconnect");
        layout.addWidget(connectButton);
        layout.addWidget(disconnectButton);
    }

    void ConnectionPanel::ConnectSignals()
    {
        connect(refreshButton, &QPushButton::clicked, this, &ConnectionPanel::RefreshInterfaces);
        connect(interfaceCombo, &QComboBox::currentTextChanged, this, &ConnectionPanel::UpdateConnectEnabled);
        connect(connectButton, &QPushButton::clicked, this, &ConnectionPanel::ConnectRequested);
        connect(disconnectButton, &QPushButton::clicked, this, &ConnectionPanel::DisconnectRequested);
        connect(nodeIdSpin, &QSpinBox::valueChanged, this, [this](int value)
            {
                emit NodeIdChanged(static_cast<uint16_t>(value));
            });
    }

    void ConnectionPanel::RefreshInterfaces()
    {
        const auto current = interfaceCombo->currentText();
        interfaceCombo->clear();

        for (const auto& name : adapter.AvailableInterfaces())
            interfaceCombo->addItem(QString::fromStdString(name));

        if (interfaceCombo->count() == 0)
            interfaceCombo->addItem("can0");

        if (!current.isEmpty())
            interfaceCombo->setCurrentText(current);
    }

    QString ConnectionPanel::InterfaceName() const
    {
        return interfaceCombo->currentText().trimmed();
    }

    uint32_t ConnectionPanel::Bitrate() const
    {
        return bitrateCombo->currentData().toUInt();
    }

    QString ConnectionPanel::BitrateText() const
    {
        return bitrateCombo->currentText();
    }

    uint16_t ConnectionPanel::NodeId() const
    {
        return static_cast<uint16_t>(nodeIdSpin->value());
    }

    void ConnectionPanel::RestoreSettings(const QSettings& settings)
    {
        const auto interfaceName = settings.value("connection/interface").toString();
        if (!interfaceName.isEmpty())
            interfaceCombo->setCurrentText(interfaceName);

        const auto bitrateIndex = bitrateCombo->findData(settings.value("connection/bitrate", Bitrate()).toUInt());
        if (bitrateIndex >= 0)
            bitrateCombo->setCurrentIndex(bitrateIndex);

        nodeIdSpin->setValue(settings.value("connection/nodeId", NodeId()).toInt());
    }

    void ConnectionPanel::SaveSettings(QSettings& settings) const
    {
        settings.setValue("connection/interface", InterfaceName());
        settings.setValue("connection/bitrate", Bitrate());
        settings.setValue("connection/nodeId", NodeId());
    }

    void ConnectionPanel::OnConnectionChanged(bool isConnected)
    {
        connected = isConnected;
        disconnectButton->setEnabled(connected);
        interfaceCombo->setEnabled(!connected);
        refreshButton->setEnabled(!connected);
        bitrateCombo->setEnabled(!connected);
        UpdateConnectEnabled();

        statusLabel->setText(connected ? QString("Connected to %1 at %2").arg(InterfaceName(), BitrateText()) : QString("Not connected"));
        ui::backend::qt::StyleStatusLabel(*statusLabel, connected ? ui::theme::StatusLevel::Ok : ui::theme::StatusLevel::Neutral);
    }

    void ConnectionPanel::UpdateConnectEnabled()
    {
        connectButton->setEnabled(!connected && !InterfaceName().isEmpty());
    }
}
