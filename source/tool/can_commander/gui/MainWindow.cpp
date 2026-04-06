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
        CanCommandClientObserver::Attach(client);
        setWindowTitle("e-foc CAN Commander");
        resize(1000, 700);

        auto* centralWidget = new QWidget();
        setCentralWidget(centralWidget);
        auto* mainLayout = new QVBoxLayout(centralWidget);

        auto* topBar = new QHBoxLayout();
        connectionPanel = new ConnectionPanel(adapter);
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

        timeoutTimer.setSingleShot(true);
        connect(&timeoutTimer, &QTimer::timeout, [this]()
            {
                client.HandleTimeout();
            });

        connect(&rxPollTimer, &QTimer::timeout, [this]()
            {
                this->adapter.ProcessReadEvent();
            });

        connect(connectionPanel, &ConnectionPanel::ConnectRequested, this, &MainWindow::OnConnect);
        connect(connectionPanel, &ConnectionPanel::DisconnectRequested, this, &MainWindow::OnDisconnect);

        connect(commandPanel, &CommandPanel::StartMotorRequested, [this]()
            {
                client.SendStartMotor();
                timeoutTimer.start(2000);
            });
        connect(commandPanel, &CommandPanel::StopMotorRequested, [this]()
            {
                client.SendStopMotor();
                timeoutTimer.start(2000);
            });
        connect(commandPanel, &CommandPanel::EmergencyStopRequested, [this]()
            {
                client.SendEmergencyStop();
                timeoutTimer.start(2000);
            });
        connect(commandPanel, &CommandPanel::SetControlModeRequested, [this](services::FocMotorMode mode)
            {
                client.SendSetControlMode(mode);
                commandPanel->SetActiveControlMode(mode);
                timeoutTimer.start(2000);
            });
        connect(commandPanel, &CommandPanel::SetTorqueSetpointRequested, [this](float iq)
            {
                client.SendSetTorqueSetpoint(iq);
                timeoutTimer.start(2000);
            });
        connect(commandPanel, &CommandPanel::SetSpeedSetpointRequested, [this](float speed)
            {
                client.SendSetSpeedSetpoint(speed);
                timeoutTimer.start(2000);
            });
        connect(commandPanel, &CommandPanel::SetPositionSetpointRequested, [this](float pos)
            {
                client.SendSetPositionSetpoint(pos);
                timeoutTimer.start(2000);
            });

        connect(commandPanel, &CommandPanel::SetCurrentIdPidRequested, [this](float kp, float ki, float kd)
            {
                client.SendSetCurrentIdPid(kp, ki, kd);
                timeoutTimer.start(2000);
            });
        connect(commandPanel, &CommandPanel::SetSpeedPidRequested, [this](float kp, float ki, float kd)
            {
                client.SendSetSpeedPid(kp, ki, kd);
                timeoutTimer.start(2000);
            });
        connect(commandPanel, &CommandPanel::SetPositionPidRequested, [this](float kp, float ki, float kd)
            {
                client.SendSetPositionPid(kp, ki, kd);
                timeoutTimer.start(2000);
            });

        connect(commandPanel, &CommandPanel::SetSupplyVoltageRequested, [this](float v)
            {
                client.SendSetSupplyVoltage(v);
            });
        connect(commandPanel, &CommandPanel::SetMaxCurrentRequested, [this](float a)
            {
                client.SendSetMaxCurrent(a);
            });

        connect(commandPanel, &CommandPanel::RequestDataRequested, [this]()
            {
                client.RequestData();
            });

        connect(nodeIdSpin, &QSpinBox::valueChanged, [this](int value)
            {
                client.SetNodeId(static_cast<uint16_t>(value));
            });
    }

    void MainWindow::OnConnect()
    {
        auto name = connectionPanel->InterfaceName().toStdString();
        adapter.Connect(infra::BoundedConstString(name.data(), name.size()), connectionPanel->Bitrate());
    }

    void MainWindow::OnDisconnect()
    {
        TeardownSocketNotifier();
        adapter.Disconnect();
    }

    void MainWindow::OnConnectionChanged(bool connected)
    {
        connectionPanel->OnConnectionChanged(connected);
        commandPanel->SetCommandsEnabled(connected);

        if (connected)
        {
            SetupSocketNotifier();
            client.RequestData();
        }
        else
        {
            TeardownSocketNotifier();
        }
    }

    void MainWindow::OnBusyChanged(bool busy)
    {
        commandPanel->SetCommandsEnabled(!busy && adapter.IsConnected());
        if (!busy)
            timeoutTimer.stop();
    }

    void MainWindow::OnCommandTimeout()
    {
        logView->appendPlainText("TIMEOUT: No response received");
    }

    void MainWindow::OnAdapterError(infra::BoundedConstString message)
    {
        logView->appendPlainText(QString("ERROR: %1").arg(QString::fromUtf8(message.data(), static_cast<int>(message.size()))));
    }

    void MainWindow::OnMotorStatusReceived(services::FocMotorState state, services::FocFaultCode fault)
    {
        telemetryPanel->OnMotorStatus(state, fault);
    }

    void MainWindow::OnCurrentMeasurementReceived(float idCurrent, float iqCurrent)
    {
        telemetryPanel->OnCurrentMeasurement(idCurrent, iqCurrent);
    }

    void MainWindow::OnSpeedPositionReceived(float speed, float position)
    {
        telemetryPanel->OnSpeedPosition(speed, position);
    }

    void MainWindow::OnBusVoltageReceived(float voltage)
    {
        telemetryPanel->OnBusVoltage(voltage);
    }

    void MainWindow::OnFaultEventReceived(services::FocFaultCode fault)
    {
        telemetryPanel->OnFaultEvent(fault);
    }

    void MainWindow::OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data)
    {
        QString hex;
        for (std::size_t i = 0; i < data.size(); ++i)
        {
            if (i > 0)
                hex += " ";
            hex += QString("%1").arg(data[i], 2, 16, QChar('0')).toUpper();
        }

        logView->appendPlainText(
            QString("[%1] ID: 0x%2  Data: [%3]")
                .arg(transmitted ? "TX" : "RX", -2)
                .arg(id, 8, 16, QChar('0'))
                .arg(hex));

        auto* scrollBar = logView->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }

    void MainWindow::SetupSocketNotifier()
    {
        int fd = adapter.FileDescriptor();
        if (fd >= 0)
        {
            socketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            connect(socketNotifier, &QSocketNotifier::activated, [this]()
                {
                    adapter.ProcessReadEvent();
                });
        }
        else
        {
            rxPollTimer.start(1); // 1 ms polling for adapters without a file descriptor (e.g. Windows)
        }
    }

    void MainWindow::TeardownSocketNotifier()
    {
        rxPollTimer.stop();
        delete socketNotifier;
        socketNotifier = nullptr;
    }
}
