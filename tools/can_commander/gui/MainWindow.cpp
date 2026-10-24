#include "tools/can_commander/gui/MainWindow.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include "tools/can_commander/logic/CommandDescriptions.hpp"
#include "tools/can_commander/support/ApplicationInfo.hpp"
#include "tools/can_commander/support/IssueReport.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QShortcut>
#include <QStatusBar>
#include <QVBoxLayout>
#include <utility>

namespace tool
{
    namespace
    {
        using ui::backend::qt::ToQt;
        using ui::theme::StatusLevel;

        constexpr int commandTimeoutMs{ 2000 };

        QString ConnectFailureHint()
        {
#if defined(_WIN32)
            return "check that the adapter is plugged in and its driver (PCAN, Kvaser or CANable) is installed";
#else
            return "check that the interface exists and is up, e.g. sudo ip link set can0 up type can bitrate 1000000";
#endif
        }
    }

    MainWindow::MainWindow(CanBusAdapter& adapter, IssueLog& issueLog, QWidget* parent)
        : QMainWindow(parent)
        , adapter(adapter)
        , issueLog(issueLog)
        , client(adapter)
        , updateChecker(ReleaseVersion::Parse(application::version).value_or(ReleaseVersion{}), QUrl{ ToQt(application::latestReleaseApiUrl) })
    {
        CanCommandClientObserver::Attach(client);
        setWindowTitle(QString("%1 %2").arg(ToQt(application::displayName), ToQt(application::version)));
        setMinimumSize(900, 600);
        resize(1200, 800);

        BuildMenus();
        BuildLayout();
        BuildStatusBar();
        RestoreSettings();
        ConnectCommands();
        ConnectSetpoints();
        ConnectUpdateChecker();

        UpdateCommandsEnabled();
        UpdateStatusBar();
    }

    MainWindow::~MainWindow()
    {
        // The client is a member and is destroyed before this observer base, and an EmIL subject
        // asserts that no observer is still attached when it goes.
        CanCommandClientObserver::Detach();
    }

    void MainWindow::CheckForUpdatesOnStartup()
    {
        if (checkAtStartupAction->isChecked())
            CheckForUpdates(false);
    }

    void MainWindow::ReportStartupProblem(const QString& title, const QString& message)
    {
        Report(IssueSeverity::error, QString("%1: %2").arg(title, message));
        QMessageBox::warning(this, title, message + "\n\nThe application stays open so you can read the log or report the issue.");
    }

    void MainWindow::closeEvent(QCloseEvent* event)
    {
        SaveSettings();
        Report(IssueSeverity::info, "Session ended");
        event->accept();
    }

    void MainWindow::BuildMenus()
    {
        BuildFileMenu();

        auto* viewMenu = menuBar()->addMenu("&View");
        showFramesAction = viewMenu->addAction("Show CAN &Frames");
        showFramesAction->setCheckable(true);
        viewMenu->addAction("&Clear Event Log", QKeySequence{ Qt::CTRL | Qt::Key_L }, this, [this]()
            {
                eventLog->Clear();
            });

        BuildHelpMenu();
    }

    void MainWindow::BuildFileMenu()
    {
        auto* fileMenu = menuBar()->addMenu("&File");
        fileMenu->addAction("&Save Event Log…", QKeySequence::Save, this, [this]()
            {
                eventLog->SaveAs();
            });
        fileMenu->addSeparator();
        fileMenu->addAction("&Quit", QKeySequence::Quit, this, &QWidget::close);
    }

    void MainWindow::BuildHelpMenu()
    {
        auto* helpMenu = menuBar()->addMenu("&Help");
        helpMenu->addAction("Check for &Updates…", this, [this]()
            {
                CheckForUpdates(true);
            });
        checkAtStartupAction = helpMenu->addAction("Check for Updates at &Startup");
        checkAtStartupAction->setCheckable(true);
        checkAtStartupAction->setChecked(true);
        helpMenu->addSeparator();
        helpMenu->addAction("&Report Issue…", this, &MainWindow::ReportIssue);
        helpMenu->addAction("Open &Log Folder", this, &MainWindow::OpenLogFolder);
        helpMenu->addSeparator();
        helpMenu->addAction(QString("&About %1").arg(ToQt(application::displayName)), this, &MainWindow::ShowAbout);
    }

    void MainWindow::BuildLayout()
    {
        auto* central = new QWidget();
        setCentralWidget(central);
        auto* layout = new QVBoxLayout(central);

        updateBanner = new UpdateBanner();
        layout->addWidget(updateBanner);
        layout->addWidget(BuildSafetyBar());

        commandPanel = new CommandPanel();
        telemetryPanel = new TelemetryPanel();
        workSplitter = new QSplitter(Qt::Horizontal);
        workSplitter->addWidget(commandPanel);
        workSplitter->addWidget(telemetryPanel);
        workSplitter->setStretchFactor(0, 3);
        workSplitter->setStretchFactor(1, 2);

        eventLog = new EventLogView();
        logSplitter = new QSplitter(Qt::Vertical);
        logSplitter->addWidget(workSplitter);
        logSplitter->addWidget(eventLog);
        logSplitter->setStretchFactor(0, 3);
        logSplitter->setStretchFactor(1, 1);
        layout->addWidget(logSplitter, 1);
    }

    QWidget* MainWindow::BuildSafetyBar()
    {
        auto* bar = new QWidget();
        auto* layout = new QHBoxLayout(bar);
        layout->setContentsMargins(0, 0, 0, 0);

        connectionPanel = new ConnectionPanel(adapter);
        layout->addWidget(connectionPanel, 1);

        emergencyStopButton = new QPushButton("EMERGENCY STOP\n(Esc)");
        emergencyStopButton->setMinimumSize(180, 56);
        emergencyStopButton->setAccessibleName("Emergency stop");
        ui::backend::qt::StyleButton(*emergencyStopButton, ui::theme::ButtonRole::EmergencyStop);
        layout->addWidget(emergencyStopButton);

        auto* shortcut = new QShortcut(QKeySequence{ Qt::Key_Escape }, this);
        shortcut->setContext(Qt::WindowShortcut);
        connect(shortcut, &QShortcut::activated, this, &MainWindow::EmergencyStop);
        connect(emergencyStopButton, &QPushButton::clicked, this, &MainWindow::EmergencyStop);
        return bar;
    }

    void MainWindow::BuildStatusBar()
    {
        activityStatus = new QLabel();
        nodeStatus = new QLabel();
        firmwareStatus = new QLabel();
        statusBar()->addPermanentWidget(activityStatus);
        statusBar()->addPermanentWidget(nodeStatus);
        statusBar()->addPermanentWidget(firmwareStatus);
    }

    void MainWindow::ConnectCommands()
    {
        timeoutTimer.setSingleShot(true);
        connect(&timeoutTimer, &QTimer::timeout, this, [this]()
            {
                client.HandleTimeout();
            });
        connect(&rxPollTimer, &QTimer::timeout, this, [this]()
            {
                adapter.ProcessReadEvent();
            });

        connect(connectionPanel, &ConnectionPanel::ConnectRequested, this, &MainWindow::Connect);
        connect(connectionPanel, &ConnectionPanel::DisconnectRequested, this, &MainWindow::Disconnect);
        connect(connectionPanel, &ConnectionPanel::NodeIdChanged, this, &MainWindow::SelectNode);
        connect(showFramesAction, &QAction::toggled, eventLog, &EventLogView::SetShowsFrames);
        connect(eventLog, &EventLogView::ShowFramesChanged, showFramesAction, &QAction::setChecked);

        connect(commandPanel, &CommandPanel::StartMotorRequested, this, [this]()
            {
                Send("Start Motor", [this]()
                    {
                        return client.SendStartMotor();
                    });
            });
        connect(commandPanel, &CommandPanel::StopMotorRequested, this, [this]()
            {
                Send("Stop Motor", [this]()
                    {
                        return client.SendStopMotor();
                    });
            });
        connect(commandPanel, &CommandPanel::RequestDataRequested, this, [this]()
            {
                client.RequestData();
            });
    }

    void MainWindow::ConnectSetpoints()
    {
        connect(commandPanel, &CommandPanel::SetControlModeRequested, this, [this](can::FocMotorMode mode)
            {
                commandPanel->ShowRequestedControlMode(mode);
                Send("Control Mode", [this, mode]()
                    {
                        return client.SendSetControlMode(mode);
                    });
            });
        connect(commandPanel, &CommandPanel::SetTorqueSetpointRequested, this, [this](float value)
            {
                Send(QString("Torque Setpoint %1 A").arg(value), [this, value]()
                    {
                        return client.SendSetTorqueSetpoint(value);
                    });
            });
        connect(commandPanel, &CommandPanel::SetSpeedSetpointRequested, this, [this](float value)
            {
                Send(QString("Speed Setpoint %1 rad/s").arg(value), [this, value]()
                    {
                        return client.SendSetSpeedSetpoint(value);
                    });
            });
        connect(commandPanel, &CommandPanel::SetPositionSetpointRequested, this, [this](float value)
            {
                Send(QString("Position Setpoint %1 rad").arg(value), [this, value]()
                    {
                        return client.SendSetPositionSetpoint(value);
                    });
            });
        connect(commandPanel, &CommandPanel::SetCurrentBandwidthRequested, this, [this](float value)
            {
                Send(QString("Current Loop Bandwidth %1 rad/s").arg(value), [this, value]()
                    {
                        return client.SendSetCurrentBandwidth(value);
                    });
            });
        connect(commandPanel, &CommandPanel::SetSpeedBandwidthRequested, this, [this](float value)
            {
                Send(QString("Speed Loop Bandwidth %1 rad/s").arg(value), [this, value]()
                    {
                        return client.SendSetSpeedBandwidth(value);
                    });
            });
        connect(commandPanel, &CommandPanel::SetPositionBandwidthRequested, this, [this](float value)
            {
                Send(QString("Position Loop Bandwidth %1 rad/s").arg(value), [this, value]()
                    {
                        return client.SendSetPositionBandwidth(value);
                    });
            });
    }

    void MainWindow::ConnectUpdateChecker()
    {
        connect(&updateChecker, &UpdateChecker::UpdateAvailable, this, [this](const ReleaseInfo& release)
            {
                userRequestedUpdateCheck = false;
                Report(IssueSeverity::info, QString("Update available: %1").arg(release.tag));
                updateBanner->ShowRelease(release);
            });
        connect(&updateChecker, &UpdateChecker::UpToDate, this, [this](const ReleaseInfo& latest)
            {
                Report(IssueSeverity::info, QString("Up to date (latest release %1)").arg(latest.tag));
                if (std::exchange(userRequestedUpdateCheck, false))
                    QMessageBox::information(this, "Check for Updates", QString("You are running the latest version (%1).").arg(ToQt(application::version)));
            });
        connect(&updateChecker, &UpdateChecker::CheckFailed, this, [this](const QString& reason)
            {
                Report(IssueSeverity::warning, QString("Update check failed: %1").arg(reason));
                if (std::exchange(userRequestedUpdateCheck, false))
                    QMessageBox::warning(this, "Check for Updates",
                        QString("Could not check for updates: %1\n\nReleases are also listed at %2").arg(reason, ToQt(application::releasesUrl)));
            });
    }

    void MainWindow::RestoreSettings()
    {
        QSettings settings;
        restoreGeometry(settings.value("window/geometry").toByteArray());
        workSplitter->restoreState(settings.value("window/workSplitter").toByteArray());
        logSplitter->restoreState(settings.value("window/logSplitter").toByteArray());
        eventLog->SetShowsFrames(settings.value("log/showFrames", false).toBool());
        showFramesAction->setChecked(eventLog->ShowsFrames());
        checkAtStartupAction->setChecked(settings.value("updates/checkAtStartup", true).toBool());
        connectionPanel->RestoreSettings(settings);
        client.SetNodeId(connectionPanel->NodeId());
    }

    void MainWindow::SaveSettings() const
    {
        QSettings settings;
        settings.setValue("window/geometry", saveGeometry());
        settings.setValue("window/workSplitter", workSplitter->saveState());
        settings.setValue("window/logSplitter", logSplitter->saveState());
        settings.setValue("log/showFrames", eventLog->ShowsFrames());
        settings.setValue("updates/checkAtStartup", checkAtStartupAction->isChecked());
        connectionPanel->SaveSettings(settings);
    }

    void MainWindow::Connect()
    {
        const auto name = connectionPanel->InterfaceName().toStdString();
        Report(IssueSeverity::info, QString("Connecting to %1 at %2").arg(connectionPanel->InterfaceName(), connectionPanel->BitrateText()));

        if (!adapter.Connect(infra::BoundedConstString(name.data(), name.size()), connectionPanel->Bitrate()))
            Report(IssueSeverity::error, QString("Could not connect to %1: %2").arg(connectionPanel->InterfaceName(), ConnectFailureHint()));
    }

    void MainWindow::Disconnect()
    {
        TeardownSocketNotifier();
        adapter.Disconnect();
    }

    void MainWindow::SelectNode(uint16_t nodeId)
    {
        client.SetNodeId(nodeId);
        ForgetNodeState();
        Report(IssueSeverity::info, QString("Commanding node %1").arg(nodeId));

        if (busConnected)
            client.RequestData();

        UpdateCommandsEnabled();
        UpdateStatusBar();
    }

    void MainWindow::ForgetNodeState()
    {
        nodeOnline.reset();
        contract = Contract::unknown;
        lastState.reset();
        lastFault.reset();
        telemetryPanel->Clear();
        commandPanel->ClearActiveControlMode();
    }

    void MainWindow::EmergencyStop()
    {
        if (!emergencyStopButton->isEnabled())
            return;

        Report(IssueSeverity::warning, "Emergency stop requested");
        Send("Emergency Stop", [this]()
            {
                return client.SendEmergencyStop();
            });
    }

    void MainWindow::Send(const QString& command, const std::function<bool()>& send)
    {
        pendingCommand = command;
        if (send())
            return;

        const auto reason = contract == Contract::incompatible ? "the firmware contract is incompatible" : "the CAN adapter refused the frame";
        Report(IssueSeverity::error, QString("%1 was not sent: %2").arg(command, reason));
    }

    void MainWindow::Report(IssueSeverity severity, const QString& message)
    {
        eventLog->Append(severity, message);
        issueLog.Write(severity, message);

        if (severity == IssueSeverity::error)
            statusBar()->showMessage(message.section('\n', 0, 0), 10000);
    }

    QString MainWindow::CommandsUnavailableReason() const
    {
        if (!busConnected)
            return "Connect to a CAN interface to send commands.";
        if (contract == Contract::incompatible)
            return QString("Firmware contract %1 is incompatible with this tool (%2.x): commands are disabled. Update the firmware or the tool.")
                .arg(contractVersion)
                .arg(can::focContractVersionMajor);
        if (contract == Contract::unknown)
            return QString("Waiting for node %1 to report its firmware version… Check the node ID if this does not clear.").arg(client.NodeId());
        if (client.IsBusy())
            return QString("Waiting for the response to %1…").arg(pendingCommand);
        return {};
    }

    void MainWindow::UpdateCommandsEnabled()
    {
        commandPanel->SetCommandsAvailable(CommandsUnavailableReason(), !client.IsBusy());

        const bool canStop = busConnected && contract != Contract::incompatible;
        emergencyStopButton->setEnabled(canStop);
        if (canStop)
            emergencyStopButton->setToolTip("Stop the motor immediately (Esc). Works while another command is pending.");
        else if (busConnected)
            emergencyStopButton->setToolTip("The firmware contract is incompatible, so the emergency stop cannot be sent. Use the hardware stop.");
        else
            emergencyStopButton->setToolTip("Connect to a CAN interface first.");
    }

    void MainWindow::UpdateStatusBar()
    {
        activityStatus->setText(client.IsBusy() ? QString("Waiting for response to %1…").arg(pendingCommand) : QString{});
        activityStatus->setVisible(client.IsBusy());

        const auto node = QString("Node %1: ").arg(client.NodeId());
        if (!busConnected)
        {
            nodeStatus->setText(node + "—");
            ui::backend::qt::StyleStatusLabel(*nodeStatus, StatusLevel::Neutral);
        }
        else if (!nodeOnline.has_value())
        {
            nodeStatus->setText(node + "no heartbeat yet");
            ui::backend::qt::StyleStatusLabel(*nodeStatus, StatusLevel::Neutral);
        }
        else
        {
            nodeStatus->setText(node + (*nodeOnline ? "online" : "offline"));
            ui::backend::qt::StyleStatusLabel(*nodeStatus, *nodeOnline ? StatusLevel::Ok : StatusLevel::Warning);
        }

        switch (contract)
        {
            case Contract::compatible:
                firmwareStatus->setText(QString("Firmware contract %1").arg(contractVersion));
                ui::backend::qt::StyleStatusLabel(*firmwareStatus, StatusLevel::Ok);
                break;
            case Contract::incompatible:
                firmwareStatus->setText(QString("Firmware contract %1: incompatible").arg(contractVersion));
                ui::backend::qt::StyleStatusLabel(*firmwareStatus, StatusLevel::Fault);
                break;
            default:
                firmwareStatus->setText("Firmware: unknown");
                ui::backend::qt::StyleStatusLabel(*firmwareStatus, StatusLevel::Neutral);
                break;
        }
    }

    void MainWindow::CheckForUpdates(bool userInitiated)
    {
        userRequestedUpdateCheck = userRequestedUpdateCheck || userInitiated;
        if (userInitiated)
            statusBar()->showMessage("Checking for updates…", 5000);
        updateChecker.Check();
    }

    void MainWindow::ReportIssue()
    {
        Report(IssueSeverity::info, "Opening a pre-filled issue report in the browser");
        const auto environment = CurrentIssueEnvironment(ToQt(application::version), issueLog.FilePath());
        const auto url = BuildIssueUrl(QUrl{ ToQt(application::newIssueUrl) }, environment, issueLog.RecentLines(60));

        if (!QDesktopServices::openUrl(url))
            QMessageBox::warning(this, "Report Issue",
                QString("Could not open a web browser.\n\nPlease report the issue at %1 and attach the log file:\n%2")
                    .arg(ToQt(application::newIssueUrl), QDir::toNativeSeparators(issueLog.FilePath())));
    }

    void MainWindow::OpenLogFolder()
    {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(issueLog.Directory())))
            QMessageBox::information(this, "Open Log Folder", QString("The log files are in:\n%1").arg(QDir::toNativeSeparators(issueLog.Directory())));
    }

    void MainWindow::ShowAbout()
    {
        QMessageBox::about(this, QString("About %1").arg(ToQt(application::displayName)),
            QString("<h3>%1 %2</h3>"
                    "<p>Commands and monitors e-foc motor controllers over CAN.</p>"
                    "<p>CAN contract %3.%4 · Qt %5</p>"
                    "<p><a href=\"%6\">Project page</a> · <a href=\"%7\">Releases</a> · <a href=\"%8\">Report an issue</a></p>"
                    "<p>Log files: %9</p>"
                    "<p>Distributed under the e-foc Non-Commercial License.</p>")
                .arg(ToQt(application::displayName), ToQt(application::version))
                .arg(can::focContractVersionMajor)
                .arg(can::focContractVersionMinor)
                .arg(QString::fromUtf8(qVersion()), ToQt(application::repositoryUrl), ToQt(application::releasesUrl), ToQt(application::newIssueUrl),
                    QDir::toNativeSeparators(issueLog.Directory()).toHtmlEscaped()));
    }

    void MainWindow::OnConnectionChanged(bool connected)
    {
        busConnected = connected;
        ForgetNodeState();
        connectionPanel->OnConnectionChanged(connected);

        if (connected)
        {
            Report(IssueSeverity::info, QString("Connected to %1 at %2").arg(connectionPanel->InterfaceName(), connectionPanel->BitrateText()));
            SetupSocketNotifier();
            client.RequestData();
        }
        else
        {
            Report(IssueSeverity::info, "Disconnected");
            TeardownSocketNotifier();
        }

        UpdateCommandsEnabled();
        UpdateStatusBar();
    }

    void MainWindow::OnNodeOnlineChanged(bool online)
    {
        if (online)
            Report(IssueSeverity::info, QString("Node %1 is online").arg(client.NodeId()));
        else
        {
            Report(IssueSeverity::warning, QString("Node %1 stopped sending heartbeats").arg(client.NodeId()));
            ForgetNodeState();
        }

        nodeOnline = online;
        UpdateCommandsEnabled();
        UpdateStatusBar();
    }

    void MainWindow::OnBusyChanged(bool busy)
    {
        if (busy)
            timeoutTimer.start(commandTimeoutMs);
        else
            timeoutTimer.stop();

        UpdateCommandsEnabled();
        UpdateStatusBar();
    }

    void MainWindow::OnCommandTimeout()
    {
        Report(IssueSeverity::warning, QString("No response to %1 within %2 s: check the node ID and that the controller is powered")
                                           .arg(pendingCommand.isEmpty() ? QString("the last command") : pendingCommand)
                                           .arg(commandTimeoutMs / 1000));
    }

    void MainWindow::OnAdapterError(infra::BoundedConstString message)
    {
        Report(IssueSeverity::error, QString("CAN adapter: %1").arg(QString::fromUtf8(message.data(), static_cast<qsizetype>(message.size()))));
    }

    void MainWindow::OnControlModeAcknowledged(can::FocMotorMode activeMode)
    {
        commandPanel->SetActiveControlMode(activeMode);
        Report(IssueSeverity::info, QString("Control mode is now %1").arg(CommandPanel::ControlModeName(activeMode)));
    }

    void MainWindow::OnCommandAck(uint8_t categoryId, uint8_t commandType, services::CanAckStatus status)
    {
        auto name = ToQt(CommandDisplayName(categoryId, commandType));
        if (name.isEmpty())
            name = QString("Command 0x%1/0x%2").arg(categoryId, 2, 16, QChar('0')).arg(commandType, 2, 16, QChar('0'));

        const auto severity = status == services::CanAckStatus::success ? IssueSeverity::info : IssueSeverity::warning;
        Report(severity, QString("%1: %2").arg(name, ToQt(AckStatusDescription(status))));
    }

    void MainWindow::OnContractVersion(uint8_t major, uint8_t minor, bool compatible)
    {
        contract = compatible ? Contract::compatible : Contract::incompatible;
        contractVersion = QString("%1.%2").arg(major).arg(minor);

        if (compatible)
            Report(IssueSeverity::info, QString("Firmware contract %1 is compatible").arg(contractVersion));
        else
            Report(IssueSeverity::error, CommandsUnavailableReason());

        UpdateCommandsEnabled();
        UpdateStatusBar();
    }

    void MainWindow::OnElectricalParametersReceived(float resistanceOhm, float inductanceMilliHenry, uint8_t polePairs)
    {
        telemetryPanel->OnElectricalParameters(resistanceOhm, inductanceMilliHenry, polePairs);
        Report(IssueSeverity::info, QString("Electrical parameters: R = %1 Ω, L = %2 mH, %3 pole pairs").arg(resistanceOhm).arg(inductanceMilliHenry).arg(polePairs));
    }

    void MainWindow::OnMechanicalParametersReceived(float frictionNmSPerRad, float inertiaKgM2)
    {
        telemetryPanel->OnMechanicalParameters(frictionNmSPerRad, inertiaKgM2);
        Report(IssueSeverity::info, QString("Mechanical parameters: B = %1 N·m·s/rad, J = %2 kg·m²").arg(frictionNmSPerRad).arg(inertiaKgM2));
    }

    void MainWindow::OnMotorStatusReceived(FocMotorState state, FocFaultCode fault)
    {
        telemetryPanel->OnMotorStatus(state, fault);

        if (lastState != state)
            Report(IssueSeverity::info, QString("Motor state: %1").arg(TelemetryPanel::MotorStateName(state)));
        lastState = state;
        OnFaultEventReceived(fault);
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

    void MainWindow::OnFaultEventReceived(FocFaultCode fault)
    {
        telemetryPanel->OnFaultEvent(fault);

        // Telemetry repeats the fault in every status frame; only a change is worth a log entry.
        if (lastFault == fault)
            return;

        if (fault != FocFaultCode::none)
            Report(IssueSeverity::error, QString("Motor fault: %1").arg(TelemetryPanel::FaultCodeName(fault)));
        else if (lastFault.has_value())
            Report(IssueSeverity::info, "Motor fault cleared");
        lastFault = fault;
    }

    void MainWindow::OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data)
    {
        eventLog->AppendFrame(transmitted, id, data);
    }

    void MainWindow::SetupSocketNotifier()
    {
        TeardownSocketNotifier();

        const int fd = adapter.FileDescriptor();
        if (fd >= 0)
        {
            socketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            connect(socketNotifier, &QSocketNotifier::activated, this, [this]()
                {
                    adapter.ProcessReadEvent();
                });
        }
        else
            rxPollTimer.start(1);
    }

    void MainWindow::TeardownSocketNotifier()
    {
        rxPollTimer.stop();
        delete socketNotifier;
        socketNotifier = nullptr;
    }
}
