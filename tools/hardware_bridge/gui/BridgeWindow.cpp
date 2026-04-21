#include "tools/hardware_bridge/gui/BridgeWindow.hpp"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

namespace tool
{
    BridgeWindow::BridgeWindow(QWidget* parent)
        : QMainWindow(parent)
    {
        setWindowTitle("e-foc Hardware Bridge");
        resize(1200, 700);

        controller = new BridgeController(this);

        auto* central = new QWidget();
        setCentralWidget(central);
        auto* root = new QVBoxLayout(central);

        // --- Connection bar ---
        auto* connGroup = new QGroupBox("Connection");
        auto* connLayout = new QHBoxLayout(connGroup);

        connLayout->addWidget(new QLabel("Host:"));
        hostInput = new QLineEdit("host.docker.internal");
        hostInput->setMinimumWidth(180);
        connLayout->addWidget(hostInput);

        connLayout->addWidget(new QLabel("Serial port:"));
        serialPortInput = new QLineEdit("5000");
        serialPortInput->setFixedWidth(60);
        connLayout->addWidget(serialPortInput);

        connLayout->addWidget(new QLabel("CAN port:"));
        canPortInput = new QLineEdit("5001");
        canPortInput->setFixedWidth(60);
        connLayout->addWidget(canPortInput);

        connectButton = new QPushButton("Connect Both");
        connLayout->addWidget(connectButton);

        disconnectButton = new QPushButton("Disconnect Both");
        disconnectButton->setEnabled(false);
        connLayout->addWidget(disconnectButton);

        statusLabel = new QLabel("Disconnected");
        statusLabel->setStyleSheet("color: red; font-weight: bold;");
        connLayout->addWidget(statusLabel);
        connLayout->addStretch();

        root->addWidget(connGroup);

        // --- Panels ---
        auto* splitter = new QSplitter(Qt::Horizontal);

        serialPanel = new SerialPanel();
        splitter->addWidget(serialPanel);

        canPanel = new CanPanel();
        splitter->addWidget(canPanel);

        splitter->setSizes({ 600, 600 });
        root->addWidget(splitter, 1);

        // --- Signal wiring ---
        connect(connectButton, &QPushButton::clicked, this, &BridgeWindow::OnConnectClicked);
        connect(disconnectButton, &QPushButton::clicked, this, &BridgeWindow::OnDisconnectClicked);

        connect(controller, &BridgeController::SerialConnected, this, &BridgeWindow::OnSerialConnected);
        connect(controller, &BridgeController::SerialDisconnected, this, &BridgeWindow::OnSerialDisconnected);
        connect(controller, &BridgeController::CanConnected, this, &BridgeWindow::OnCanConnected);
        connect(controller, &BridgeController::CanDisconnected, this, &BridgeWindow::OnCanDisconnected);
        connect(controller, &BridgeController::ConnectionError, this, &BridgeWindow::OnConnectionError);

        connect(controller, &BridgeController::SerialDataReceived, serialPanel, &SerialPanel::AppendText);

        connect(controller, &BridgeController::CanFrameReceived,
            [this](quint32 id, bool extended, QByteArray data)
            {
                canPanel->AppendFrame(true, id, extended, data);
            });

        connect(serialPanel, &SerialPanel::SendRequested, controller, &BridgeController::SendSerial);

        connect(canPanel, &CanPanel::SendRequested,
            [this](quint32 id, bool extended, QByteArray data)
            {
                controller->SendCan(id, extended, data);
                canPanel->AppendFrame(false, id, extended, data);
            });
    }

    void BridgeWindow::OnConnectClicked()
    {
        bool serialOk = false;
        bool canOk = false;
        const int serialPort = serialPortInput->text().toInt(&serialOk);
        const int canPort = canPortInput->text().toInt(&canOk);

        if (!serialOk || serialPort <= 0 || serialPort > 65535 || !canOk || canPort <= 0 || canPort > 65535)
        {
            QMessageBox::warning(this, "Invalid Port", "Serial and CAN ports must be valid TCP port numbers (1–65535).");
            return;
        }

        connectButton->setEnabled(false);
        disconnectButton->setEnabled(true);
        hostInput->setEnabled(false);
        serialPortInput->setEnabled(false);
        canPortInput->setEnabled(false);

        controller->Connect(hostInput->text().trimmed(),
            static_cast<quint16>(serialPort),
            static_cast<quint16>(canPort));
    }

    void BridgeWindow::OnDisconnectClicked()
    {
        controller->Disconnect();

        serialConnected = false;
        canConnected = false;

        serialPanel->SetConnected(false);
        canPanel->SetConnected(false);

        connectButton->setEnabled(true);
        disconnectButton->setEnabled(false);
        hostInput->setEnabled(true);
        serialPortInput->setEnabled(true);
        canPortInput->setEnabled(true);

        UpdateConnectionStatus();
    }

    void BridgeWindow::OnSerialConnected()
    {
        serialConnected = true;
        serialPanel->SetConnected(true);
        UpdateConnectionStatus();
    }

    void BridgeWindow::OnSerialDisconnected()
    {
        serialConnected = false;
        serialPanel->SetConnected(false);
        UpdateConnectionStatus();
    }

    void BridgeWindow::OnCanConnected()
    {
        canConnected = true;
        canPanel->SetConnected(true);
        UpdateConnectionStatus();
    }

    void BridgeWindow::OnCanDisconnected()
    {
        canConnected = false;
        canPanel->SetConnected(false);
        UpdateConnectionStatus();
    }

    void BridgeWindow::OnConnectionError(const QString& message)
    {
        QMessageBox::warning(this, "Connection Error", message);
        OnDisconnectClicked();
    }

    void BridgeWindow::UpdateConnectionStatus()
    {
        if (serialConnected && canConnected)
        {
            statusLabel->setText("Connected (Serial + CAN)");
            statusLabel->setStyleSheet("color: green; font-weight: bold;");
        }
        else if (serialConnected || canConnected)
        {
            const QString which = serialConnected ? "Serial only" : "CAN only";
            statusLabel->setText(QString("Partially connected (%1)").arg(which));
            statusLabel->setStyleSheet("color: orange; font-weight: bold;");
        }
        else
        {
            statusLabel->setText("Disconnected");
            statusLabel->setStyleSheet("color: red; font-weight: bold;");
        }
    }
}
