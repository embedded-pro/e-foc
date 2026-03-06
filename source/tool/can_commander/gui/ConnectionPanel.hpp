#pragma once

#include "source/tool/can_commander/adapter/CanBusAdapter.hpp"
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

namespace tool
{
    class ConnectionPanel : public QGroupBox
    {
        Q_OBJECT

    public:
        explicit ConnectionPanel(QWidget* parent = nullptr);

        QString InterfaceName() const;
        uint32_t Bitrate() const;

    signals:
        void ConnectRequested();
        void DisconnectRequested();

    public slots:
        void OnConnectionChanged(bool connected);

    private:
        QLineEdit* interfaceEdit;
        QComboBox* bitrateCombo;
        QPushButton* connectButton;
        QPushButton* disconnectButton;
        QLabel* statusLabel;
    };
}
