#include "source/tool/can_commander/gui/MainWindow.hpp"
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QScrollBar>
#include <QSplitter>
#include <QVBoxLayout>

namespace tool
{
    MainWindow::MainWindow(CanBusAdapter& adapter, QWidget* parent)
        : QMainWindow(parent)
        , adapter(adapter)
        , client(adapter)
    {
        setWindowTitle("e-foc CAN Commander");
        resize(1000, 700);

        auto* centralWidget = new QWidget();
        setCentralWidget(centralWidget);
        auto* mainLayout = new QVBoxLayout(centralWidget);

        auto* topBar = new QHBoxLayout();
        connectionPanel = new ConnectionPanel();
        topBar->addWidget(connectionPanel);

        auto* nodeGroup = new QGroupBox("Node");
        auto* nodeLayout = new QHBoxLayout(nodeGroup);
        nodeIdSpin = new QSpinBox();
        nodeIdSpin->setRange(1, 4095);
        nodeIdSpin->setValue(1);
        nodeLayout->addWidget(new QLabel("Node ID:"));
        nodeLayout->addWidget(nodeIdSpin);
        topBar->addWidget(nodeGroup);
        topBar->addStretch();
        mainLayout->addLayout(topBar);

        auto* splitter = new QSplitter(Qt::Horizontal);

        commandPanel = new CommandPanel();
        commandPanel->SetCommandsEnabled(false);
        splitter->addWidget(commandPanel);

        telemetryPanel = new TelemetryPanel();
        splitter->addWidget(telemetryPanel);

        splitter->setSizes({ 600, 400 });
        mainLayout->addWidget(splitter, 1);

        logView = new QPlainTextEdit();
        logView->setReadOnly(true);
        logView->setMaximumHeight(150);
        logView->setFont(QFont("Monospace", 9));
        mainLayout->addWidget(logView);

        ConnectSignals();
    }

    void MainWindow::ConnectSignals()
    {
        connect(connectionPanel, &ConnectionPanel::ConnectRequested, this, &MainWindow::OnConnect);
        connect(connectionPanel, &ConnectionPanel::DisconnectRequested, this, &MainWindow::OnDisconnect);

        connect(&adapter, &CanBusAdapter::ConnectionChanged, this, &MainWindow::OnConnectionChanged);
        connect(&adapter, &CanBusAdapter::ErrorOccurred, this, &MainWindow::OnError);

        connect(&client, &CanCommandClient::BusyChanged, this, &MainWindow::OnBusyChanged);
        connect(&client, &CanCommandClient::CommandTimeout, this, &MainWindow::OnCommandTimeout);
        connect(&client, &CanCommandClient::FrameLog, this, &MainWindow::OnFrameLog);

        connect(&client, &CanCommandClient::MotorStatusReceived, telemetryPanel, &TelemetryPanel::OnMotorStatus);
        connect(&client, &CanCommandClient::CurrentMeasurementReceived, telemetryPanel, &TelemetryPanel::OnCurrentMeasurement);
        connect(&client, &CanCommandClient::SpeedPositionReceived, telemetryPanel, &TelemetryPanel::OnSpeedPosition);
        connect(&client, &CanCommandClient::BusVoltageReceived, telemetryPanel, &TelemetryPanel::OnBusVoltage);
        connect(&client, &CanCommandClient::FaultEventReceived, telemetryPanel, &TelemetryPanel::OnFaultEvent);
        connect(&client, &CanCommandClient::HeartbeatReceived, telemetryPanel, &TelemetryPanel::OnHeartbeat);
        connect(&client, &CanCommandClient::CommandAckReceived, telemetryPanel, &TelemetryPanel::OnCommandAck);

        connect(commandPanel, &CommandPanel::StartMotorRequested, &client, &CanCommandClient::SendStartMotor);
        connect(commandPanel, &CommandPanel::StopMotorRequested, &client, &CanCommandClient::SendStopMotor);
        connect(commandPanel, &CommandPanel::EmergencyStopRequested, &client, &CanCommandClient::SendEmergencyStop);
        connect(commandPanel, &CommandPanel::SetControlModeRequested, &client, &CanCommandClient::SendSetControlMode);
        connect(commandPanel, &CommandPanel::SetTorqueSetpointRequested, &client, &CanCommandClient::SendSetTorqueSetpoint);
        connect(commandPanel, &CommandPanel::SetSpeedSetpointRequested, &client, &CanCommandClient::SendSetSpeedSetpoint);
        connect(commandPanel, &CommandPanel::SetPositionSetpointRequested, &client, &CanCommandClient::SendSetPositionSetpoint);

        connect(commandPanel, &CommandPanel::SetCurrentIdPidRequested, &client, &CanCommandClient::SendSetCurrentIdPid);
        connect(commandPanel, &CommandPanel::SetCurrentIqPidRequested, &client, &CanCommandClient::SendSetCurrentIqPid);
        connect(commandPanel, &CommandPanel::SetSpeedPidRequested, &client, &CanCommandClient::SendSetSpeedPid);
        connect(commandPanel, &CommandPanel::SetPositionPidRequested, &client, &CanCommandClient::SendSetPositionPid);

        connect(commandPanel, &CommandPanel::SetPolePairsRequested, &client, &CanCommandClient::SendSetPolePairs);
        connect(commandPanel, &CommandPanel::SetResistanceRequested, &client, &CanCommandClient::SendSetResistance);
        connect(commandPanel, &CommandPanel::SetInductanceRequested, &client, &CanCommandClient::SendSetInductance);
        connect(commandPanel, &CommandPanel::SetFluxLinkageRequested, &client, &CanCommandClient::SendSetFluxLinkage);

        connect(commandPanel, &CommandPanel::SetSupplyVoltageRequested, &client, &CanCommandClient::SendSetSupplyVoltage);
        connect(commandPanel, &CommandPanel::SetMaxCurrentRequested, &client, &CanCommandClient::SendSetMaxCurrent);

        connect(commandPanel, &CommandPanel::SendHeartbeatRequested, &client, &CanCommandClient::SendHeartbeat);

        connect(commandPanel, &CommandPanel::RequestStatusRequested, &client, &CanCommandClient::SendRequestStatus);

        connect(&client, &CanCommandClient::ControlModeReceived, commandPanel, &CommandPanel::SetActiveControlMode);

        connect(nodeIdSpin, &QSpinBox::valueChanged, [this](int value)
            {
                client.SetNodeId(static_cast<uint16_t>(value));
            });
    }

    void MainWindow::OnConnect()
    {
        adapter.Connect(connectionPanel->InterfaceName(), connectionPanel->Bitrate());
    }

    void MainWindow::OnDisconnect()
    {
        adapter.Disconnect();
    }

    void MainWindow::OnConnectionChanged(bool connected)
    {
        connectionPanel->OnConnectionChanged(connected);
        commandPanel->SetCommandsEnabled(connected);

        if (connected)
            client.SendRequestStatus();
    }

    void MainWindow::OnBusyChanged(bool busy)
    {
        commandPanel->SetCommandsEnabled(!busy && adapter.IsConnected());
    }

    void MainWindow::OnCommandTimeout()
    {
        logView->appendPlainText("TIMEOUT: No response received");
    }

    void MainWindow::OnError(QString message)
    {
        logView->appendPlainText("ERROR: " + message);
    }

    void MainWindow::OnFrameLog(QString direction, uint32_t id, QByteArray data)
    {
        QString hex;
        for (int i = 0; i < data.size(); ++i)
        {
            if (i > 0)
                hex += " ";
            hex += QString("%1").arg(static_cast<uint8_t>(data[i]), 2, 16, QChar('0')).toUpper();
        }

        logView->appendPlainText(
            QString("[%1] ID: 0x%2  Data: [%3]")
                .arg(direction, -2)
                .arg(id, 8, 16, QChar('0'))
                .arg(hex));

        auto* scrollBar = logView->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}
