#pragma once

#include "tools/hardware_bridge/client/gui/BridgeController.hpp"
#include "tools/hardware_bridge/client/gui/CanPanel.hpp"
#include "tools/hardware_bridge/client/gui/SerialPanel.hpp"
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QString>

namespace tool
{
    class BridgeWindow
        : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit BridgeWindow(QWidget* parent = nullptr);

    private slots:
        void OnConnectClicked();
        void OnDisconnectClicked();
        void OnSerialConnected();
        void OnSerialDisconnected();
        void OnCanConnected();
        void OnCanDisconnected();
        void OnConnectionError(const QString& message);

    private:
        void UpdateConnectionStatus();

        BridgeController* controller;

        QLineEdit* hostInput;
        QLineEdit* serialPortInput;
        QLineEdit* canPortInput;
        QPushButton* connectButton;
        QPushButton* disconnectButton;
        QLabel* statusLabel;

        SerialPanel* serialPanel;
        CanPanel* canPanel;

        bool serialConnected{ false };
        bool canConnected{ false };
    };
}
