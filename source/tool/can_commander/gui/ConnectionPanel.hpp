#pragma once

#include "source/tool/can_commander/adapter/CanBusAdapter.hpp"
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

namespace tool
{
    class ConnectionPanel : public QGroupBox
    {
        Q_OBJECT

    public:
        explicit ConnectionPanel(CanBusAdapter& adapter, QWidget* parent = nullptr);

        QString InterfaceName() const;
        uint32_t Bitrate() const;

    signals:
        void ConnectRequested();
        void DisconnectRequested();

    public slots:
        void OnConnectionChanged(bool connected);

    private slots:
        void RefreshInterfaces();

    private:
        CanBusAdapter& adapter;
        QComboBox* interfaceCombo;
        QPushButton* refreshButton;
        QComboBox* bitrateCombo;
        QPushButton* connectButton;
        QPushButton* disconnectButton;
        QLabel* statusLabel;
    };
}
