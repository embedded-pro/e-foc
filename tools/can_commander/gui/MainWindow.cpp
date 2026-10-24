#include "tools/can_commander/gui/MainWindow.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include "tools/can_commander/support/ApplicationInfo.hpp"
#include "tools/can_commander/support/IssueReport.hpp"
#include "tools/can_commander/support/UpdateInstallation.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>
#include <utility>

namespace tool
{
    namespace
    {
        using ui::backend::qt::ToQt;
        using ui::theme::StatusLevel;

        QString ConnectFailureHint(const QString& interfaceName)
        {
            if (interfaceName.startsWith("slcan:", Qt::CaseInsensitive) || interfaceName.startsWith("CANable:", Qt::CaseInsensitive))
                return "check the serial port name, that the adapter runs slcan firmware and that no other program has the port open";
            if (interfaceName.startsWith("candleLight:", Qt::CaseInsensitive))
                return "check that the candleLight adapter is plugged in and not used by another program";
            if (interfaceName.startsWith("PCAN:", Qt::CaseInsensitive) || interfaceName.startsWith("Kvaser:", Qt::CaseInsensitive))
                return "check that the adapter is plugged in and the vendor driver is installed";
            return "check that the interface exists and is up, e.g. sudo ip link set can0 up type can bitrate 1000000";
        }

        constexpr std::chrono::hours automaticUpdateInterval{ 24 };

        QString UpdateDownloadDirectory()
        {
            return QDir{ QStandardPaths::writableLocation(QStandardPaths::CacheLocation) }.filePath("updates");
        }

        void RemoveStaleUpdateDownloads()
        {
            const auto cutoff = QDateTime::currentDateTimeUtc().addDays(-1);
            QDirIterator files{ UpdateDownloadDirectory(), QDir::Files };
            while (files.hasNext())
                if (const auto file = files.nextFileInfo(); file.lastModified().toUTC() < cutoff)
                    QFile::remove(file.filePath());
        }

        std::optional<InstallCommand> UpdateCommandFor(const QString& installerPath)
        {
            return DefaultUpdateCommand(CurrentInstallerPlatform(), installerPath, QCoreApplication::applicationFilePath(), QCoreApplication::applicationPid());
        }

        QString InstallSteps()
        {
            if (CurrentInstallerPlatform() == InstallerPlatform::windowsX64)
                return "CAN Commander downloads the installer and checks it against the release. It then closes, Windows asks for permission, and the new version starts when the installation is done.";

            return "CAN Commander downloads the package and checks it against the release, then asks for your password to install it and restarts.";
        }

        QString NodeList(const std::vector<uint16_t>& nodes)
        {
            QStringList names;
            for (auto node : nodes)
                names.append(QString::number(node));
            return names.join(", ");
        }
    }

    MainWindow::MainWindow(services::CanBusAdapter& adapter, IssueLog& issueLog, QWidget* parent)
        : QMainWindow(parent)
        , adapter(adapter)
        , issueLog(issueLog)
        , client(adapter)
        , updateChecker(ReleaseVersion::Parse(application::version).value_or(ReleaseVersion{}), QUrl{ ToQt(application::latestReleaseApiUrl) })
        , updateSession(UpdateDownloadDirectory(), UpdateCommandFor)
    {
        CanCommandClientObserver::Attach(client);
        setWindowTitle(QString("%1 %2").arg(ToQt(application::displayName), ToQt(application::version)));
        setMinimumSize(900, 600);
        resize(1280, 860);

        BuildMenus();
        BuildLayout();
        statusBar()->addPermanentWidget(motorsStatus = new QLabel());
        RestoreSettings();
        ConnectSignals();
        ConnectUpdateChecker();
        ConnectUpdateSession();
        RemoveStaleUpdateDownloads();

        UpdateSafetyControls();
        UpdateStatusBar();
    }

    MainWindow::~MainWindow()
    {
        CanCommandClientObserver::Detach();
    }

    void MainWindow::StartAutomaticUpdateChecks()
    {
        connect(&automaticUpdateTimer, &QTimer::timeout, this, [this]()
            {
                if (automaticUpdatesAction->isChecked())
                    CheckForUpdates(false);
            });
        automaticUpdateTimer.start(automaticUpdateInterval);

        if (automaticUpdatesAction->isChecked())
            CheckForUpdates(false);
    }

    void MainWindow::ReportStartupProblem(const QString& title, const QString& message)
    {
        Report(IssueSeverity::error, QString("%1: %2").arg(title, message.simplified()));
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
        BuildMotorMenu();

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

    void MainWindow::BuildMotorMenu()
    {
        auto* motorMenu = menuBar()->addMenu("&Motor");
        addMotorAction = motorMenu->addAction("&Add Motor…", QKeySequence::New, this, &MainWindow::PromptAddMotor);
        motorMenu->addAction("&Close Motor Tab", QKeySequence::Close, this, [this]()
            {
                CloseMotor(motorTabs->currentIndex());
            });
        motorMenu->addSeparator();
        emergencyStopAction = motorMenu->addAction("&Emergency Stop All Motors", QKeySequence{ Qt::Key_Escape }, this, &MainWindow::EmergencyStopAll);
    }

    void MainWindow::BuildHelpMenu()
    {
        auto* helpMenu = menuBar()->addMenu("&Help");
        helpMenu->addAction("Check for &Updates…", this, [this]()
            {
                CheckForUpdates(true);
            });
        automaticUpdatesAction = helpMenu->addAction("Check for Updates &Automatically");
        automaticUpdatesAction->setCheckable(true);
        automaticUpdatesAction->setChecked(true);
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

        overview = new MotorOverview();
        layout->addWidget(overview);

        eventLog = new EventLogView();
        logSplitter = new QSplitter(Qt::Vertical);
        logSplitter->addWidget(BuildMotorArea());
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

        emergencyStopButton = new QPushButton("EMERGENCY STOP\nALL MOTORS (Esc)");
        emergencyStopButton->setMinimumSize(200, 56);
        emergencyStopButton->setAccessibleName("Emergency stop all motors");
        ui::backend::qt::StyleButton(*emergencyStopButton, ui::theme::ButtonRole::EmergencyStop);
        layout->addWidget(emergencyStopButton);

        connect(emergencyStopButton, &QPushButton::clicked, this, &MainWindow::EmergencyStopAll);
        return bar;
    }

    QWidget* MainWindow::BuildMotorArea()
    {
        motorTabs = new QTabWidget();
        motorTabs->setTabsClosable(true);
        motorTabs->setMovable(true);
        motorTabs->setDocumentMode(true);

        addMotorButton = new QPushButton("+ Add Motor…");
        addMotorButton->setToolTip("Add a tab for a motor controller by its CAN node ID");
        motorTabs->setCornerWidget(addMotorButton, Qt::TopRightCorner);

        motorArea = new QStackedWidget();
        motorArea->addWidget(BuildEmptyState());
        motorArea->addWidget(motorTabs);
        return motorArea;
    }

    QWidget* MainWindow::BuildEmptyState()
    {
        auto* page = new QWidget();
        auto* layout = new QVBoxLayout(page);
        layout->addStretch();

        auto* message = new QLabel("No motors yet.\n\nConnect to a CAN interface: every motor controller whose heartbeat is seen gets its own tab.\n"
                                   "You can also add a motor by its node ID before it comes online.");
        message->setAlignment(Qt::AlignCenter);
        message->setWordWrap(true);
        layout->addWidget(message);

        auto* addButton = new QPushButton("Add Motor…");
        ui::backend::qt::StyleButton(*addButton, ui::theme::ButtonRole::Primary);
        connect(addButton, &QPushButton::clicked, this, &MainWindow::PromptAddMotor);
        auto* buttonRow = new QHBoxLayout();
        buttonRow->addStretch();
        buttonRow->addWidget(addButton);
        buttonRow->addStretch();
        layout->addLayout(buttonRow);
        layout->addStretch();
        return page;
    }

    void MainWindow::ConnectSignals()
    {
        connect(&rxPollTimer, &QTimer::timeout, this, [this]()
            {
                adapter.ProcessReadEvent();
            });

        connect(connectionPanel, &ConnectionPanel::ConnectRequested, this, &MainWindow::Connect);
        connect(connectionPanel, &ConnectionPanel::DisconnectRequested, this, &MainWindow::Disconnect);
        connect(addMotorButton, &QPushButton::clicked, this, &MainWindow::PromptAddMotor);
        connect(motorTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::CloseMotor);
        connect(motorTabs, &QTabWidget::currentChanged, this, [this](int index)
            {
                if (auto* motor = qobject_cast<MotorPanel*>(motorTabs->widget(index)))
                    overview->SetCurrent(motor->NodeId());
            });
        connect(overview, &MotorOverview::MotorSelected, this, &MainWindow::SelectMotor);
        connect(showFramesAction, &QAction::toggled, eventLog, &EventLogView::SetShowsFrames);
        connect(eventLog, &EventLogView::ShowFramesChanged, showFramesAction, &QAction::setChecked);
    }

    void MainWindow::ConnectUpdateChecker()
    {
        connect(&updateChecker, &UpdateChecker::UpdateAvailable, this, [this](const ReleaseInfo& release)
            {
                const auto requested = std::exchange(userRequestedUpdateCheck, false);
                if (!requested && QSettings{}.value("updates/skippedVersion").toString() == release.tag)
                {
                    Report(IssueSeverity::info, QString("Update available: %1 (skipped)").arg(release.tag));
                    return;
                }

                Report(IssueSeverity::info, QString("Update available: %1").arg(release.tag));
                updateSession.Offer(release);
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

    void MainWindow::ConnectUpdateSession()
    {
        connect(&updateSession, &UpdateSession::StateChanged, this, &MainWindow::OnUpdateStateChanged);
        connect(&updateSession, &UpdateSession::RestartRequested, this, &MainWindow::RestartForUpdate);
        connect(&updateSession, &UpdateSession::Relaunch, this, &MainWindow::RelaunchUpdatedVersion);
        connect(updateBanner, &UpdateBanner::InstallRequested, this, &MainWindow::RequestUpdateInstall);
        connect(updateBanner, &UpdateBanner::CancelRequested, &updateSession, &UpdateSession::Cancel);
        connect(updateBanner, &UpdateBanner::DismissRequested, &updateSession, &UpdateSession::Dismiss);
        connect(updateBanner, &UpdateBanner::SkipRequested, this, [this]()
            {
                if (const auto& release = updateSession.State().release)
                {
                    QSettings{}.setValue("updates/skippedVersion", release->tag);
                    Report(IssueSeverity::info, QString("Update %1 skipped").arg(release->tag));
                }
                updateSession.Dismiss();
            });
    }

    void MainWindow::OnUpdateStateChanged(const UpdateState& state)
    {
        updateBanner->Render(state);

        if (std::exchange(updatePhase, state.phase) == state.phase || !state.release)
            return;

        const auto& tag = state.release->tag;
        switch (state.phase)
        {
            case UpdatePhase::downloading:
                Report(IssueSeverity::info, QString("Downloading update %1 from %2").arg(tag, state.release->installer->url.toString()));
                break;
            case UpdatePhase::ready:
                Report(IssueSeverity::info, QString("Update %1 downloaded and verified; waiting for the bus to be disconnected").arg(tag));
                break;
            case UpdatePhase::installing:
                Report(IssueSeverity::info, QString("Installing update %1").arg(tag));
                break;
            case UpdatePhase::failed:
                Report(IssueSeverity::warning, QString("Update to %1 failed: %2").arg(tag, state.failure));
                break;
            default:
                break;
        }
    }

    void MainWindow::RequestUpdateInstall()
    {
        const auto& state = updateSession.State();
        if (state.phase == UpdatePhase::available || state.phase == UpdatePhase::failed)
        {
            QMessageBox confirm{ this };
            confirm.setIcon(QMessageBox::Question);
            confirm.setWindowTitle("Install Update");
            confirm.setText(QString("Install %1 %2?").arg(ToQt(application::displayName), QString::fromStdString(state.release->version.ToString())));
            confirm.setInformativeText(InstallSteps() + (busConnected ? "\n\nInstalling waits until you disconnect from the CAN bus." : QString{}));
            auto* install = confirm.addButton("&Install and Restart", QMessageBox::AcceptRole);
            confirm.addButton(QMessageBox::Cancel);
            confirm.setDefaultButton(install);
            confirm.exec();
            if (confirm.clickedButton() != install)
                return;
        }

        updateSession.Install();
    }

    void MainWindow::RestartForUpdate()
    {
        Report(IssueSeverity::info, QString("Closing so the installer can update to %1").arg(updateSession.State().release->tag));
        close();
        QApplication::quit();
    }

    void MainWindow::RelaunchUpdatedVersion()
    {
        Report(IssueSeverity::info, QString("Update %1 installed; restarting").arg(updateSession.State().release->tag));
        SaveSettings();
        if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {}))
        {
            Report(IssueSeverity::warning, "Could not start the new version; restart CAN Commander to use it");
            return;
        }

        close();
        QApplication::quit();
    }

    void MainWindow::RestoreSettings()
    {
        QSettings settings;
        restoreGeometry(settings.value("window/geometry").toByteArray());
        logSplitter->restoreState(settings.value("window/logSplitter").toByteArray());
        eventLog->SetShowsFrames(settings.value("log/showFrames", false).toBool());
        showFramesAction->setChecked(eventLog->ShowsFrames());
        automaticUpdatesAction->setChecked(settings.value("updates/checkAtStartup", true).toBool());
        connectionPanel->RestoreSettings(settings);

        for (const auto& node : settings.value("motors/nodeIds").toList())
            if (const auto nodeId = node.toUInt(); nodeId >= 1 && nodeId <= 4095)
                AddMotor(static_cast<uint16_t>(nodeId));
    }

    void MainWindow::SaveSettings() const
    {
        QSettings settings;
        settings.setValue("window/geometry", saveGeometry());
        settings.setValue("window/logSplitter", logSplitter->saveState());
        settings.setValue("log/showFrames", eventLog->ShowsFrames());
        settings.setValue("updates/checkAtStartup", automaticUpdatesAction->isChecked());
        connectionPanel->SaveSettings(settings);

        QVariantList nodes;
        for (const auto& [nodeId, motor] : motors)
            nodes.append(nodeId);
        settings.setValue("motors/nodeIds", nodes);
    }

    void MainWindow::Connect()
    {
        const auto name = connectionPanel->InterfaceName().toStdString();
        Report(IssueSeverity::info, QString("Connecting to %1 at %2").arg(connectionPanel->InterfaceName(), connectionPanel->BitrateText()));

        if (!adapter.Connect(infra::BoundedConstString(name.data(), name.size()), connectionPanel->Bitrate()))
            Report(IssueSeverity::error, QString("Could not connect to %1: %2").arg(connectionPanel->InterfaceName(), ConnectFailureHint(connectionPanel->InterfaceName())));
    }

    void MainWindow::Disconnect()
    {
        TeardownSocketNotifier();
        adapter.Disconnect();
    }

    void MainWindow::PromptAddMotor()
    {
        if (motors.size() >= maxMotors)
        {
            QMessageBox::information(this, "Add Motor", QString("The tool follows up to %1 motors on one bus. Close a motor tab first.").arg(maxMotors));
            return;
        }

        uint16_t suggestion{ 1 };
        while (motors.contains(suggestion))
            ++suggestion;

        bool accepted{ false };
        const auto nodeId = QInputDialog::getInt(this, "Add Motor", "CAN node ID of the motor controller:", suggestion, 1, 4095, 1, &accepted);
        if (accepted)
            SelectMotor(AddMotor(static_cast<uint16_t>(nodeId))->NodeId());
    }

    MotorPanel* MainWindow::AddMotor(uint16_t nodeId)
    {
        if (auto* existing = Motor(nodeId))
            return existing;

        auto* motor = new MotorPanel(client, nodeId, [this](IssueSeverity severity, const QString& message)
            {
                Report(severity, message);
            });
        motors.emplace(nodeId, motor);
        motor->SetBusConnected(busConnected);
        if (busConnected && client.IsOnline(nodeId))
            motor->OnNodeOnlineChanged(true);

        connect(motor, &MotorPanel::StatusChanged, this, [this, motor]()
            {
                RefreshMotor(*motor);
            });

        motorTabs->addTab(motor, StatusIcon(motor->Level()), motor->Title());
        motorArea->setCurrentWidget(motorTabs);
        RefreshMotor(*motor);
        return motor;
    }

    void MainWindow::CloseMotor(int tabIndex)
    {
        auto* motor = qobject_cast<MotorPanel*>(motorTabs->widget(tabIndex));
        if (motor == nullptr)
            return;

        const auto nodeId = motor->NodeId();
        Report(IssueSeverity::info, QString("%1 tab closed; the motor itself is not stopped").arg(motor->Title()));
        motors.erase(nodeId);
        overview->RemoveMotor(nodeId);
        motorTabs->removeTab(tabIndex);
        motor->deleteLater();

        if (motors.empty())
            motorArea->setCurrentIndex(0);
        UpdateSafetyControls();
        UpdateStatusBar();
    }

    void MainWindow::SelectMotor(uint16_t nodeId)
    {
        if (auto* motor = Motor(nodeId))
            motorTabs->setCurrentWidget(motor);
    }

    MotorPanel* MainWindow::Motor(uint16_t nodeId) const
    {
        const auto motor = motors.find(nodeId);
        return motor != motors.end() ? motor->second : nullptr;
    }

    void MainWindow::RefreshMotor(MotorPanel& motor)
    {
        const auto index = motorTabs->indexOf(&motor);
        if (index >= 0)
        {
            motorTabs->setTabIcon(index, StatusIcon(motor.Level()));
            motorTabs->setTabToolTip(index, QString("%1: %2").arg(motor.Title(), motor.Summary()));
        }

        overview->SetMotor(motor.NodeId(), motor.Title(), motor.Summary(), motor.Level());
        if (motorTabs->currentWidget() == &motor)
            overview->SetCurrent(motor.NodeId());

        UpdateSafetyControls();
        UpdateStatusBar();
    }

    void MainWindow::EmergencyStopAll()
    {
        if (!emergencyStopButton->isEnabled())
            return;

        auto targets = client.OnlineNodes();
        for (const auto& [nodeId, motor] : motors)
            targets.insert(nodeId);

        std::vector<uint16_t> sent;
        std::vector<uint16_t> refused;
        for (auto nodeId : targets)
        {
            auto* motor = Motor(nodeId);
            const bool accepted = motor != nullptr ? motor->SendEmergencyStop() : client.SendEmergencyStop(nodeId);
            (accepted ? sent : refused).push_back(nodeId);
        }

        Report(IssueSeverity::warning, QString("Emergency stop sent to motor(s) %1").arg(sent.empty() ? QString("none") : NodeList(sent)));
        if (!refused.empty())
            Report(IssueSeverity::error, QString("Emergency stop could not be sent to motor(s) %1 (incompatible firmware or adapter refused): use the hardware stop").arg(NodeList(refused)));
    }

    void MainWindow::Report(IssueSeverity severity, const QString& message)
    {
        eventLog->Append(severity, message);
        issueLog.Write(severity, message);

        if (severity == IssueSeverity::error)
            statusBar()->showMessage(message.section('\n', 0, 0), 10000);
    }

    void MainWindow::UpdateSafetyControls()
    {
        bool anyStoppable = !client.OnlineNodes().empty();
        for (const auto& [nodeId, motor] : motors)
            anyStoppable = anyStoppable || motor->AcceptsEmergencyStop();

        const bool canStop = busConnected && anyStoppable;
        emergencyStopButton->setEnabled(canStop);
        emergencyStopAction->setEnabled(canStop);
        if (canStop)
            emergencyStopButton->setToolTip("Stop every motor on the bus immediately (Esc). Works while other commands are pending.");
        else if (busConnected)
            emergencyStopButton->setToolTip("No motor can receive an emergency stop: none is known, or all have incompatible firmware. Use the hardware stop.");
        else
            emergencyStopButton->setToolTip("Connect to a CAN interface first.");

        const bool canAdd = motors.size() < maxMotors;
        addMotorAction->setEnabled(canAdd);
        addMotorButton->setEnabled(canAdd);
        addMotorButton->setToolTip(canAdd ? QString("Add a tab for a motor controller by its CAN node ID") : QString("Up to %1 motors per bus").arg(maxMotors));
    }

    void MainWindow::UpdateStatusBar()
    {
        if (!busConnected)
        {
            motorsStatus->setText(QString("%1 motor tab(s) · bus not connected").arg(motors.size()));
            ui::backend::qt::StyleStatusLabel(*motorsStatus, StatusLevel::Neutral);
            return;
        }

        std::size_t faulted{ 0 };
        std::size_t online{ 0 };
        for (const auto& [nodeId, motor] : motors)
        {
            faulted += motor->Level() == StatusLevel::Fault ? 1 : 0;
            online += client.IsOnline(nodeId) ? 1 : 0;
        }

        const auto faultSummary = faulted > 0 ? QString(" · %1 in fault").arg(faulted) : QString{};
        motorsStatus->setText(QString("%1 of %2 motor(s) online%3").arg(online).arg(motors.size()).arg(faultSummary));
        ui::backend::qt::StyleStatusLabel(*motorsStatus, faulted > 0 ? StatusLevel::Fault : StatusLevel::Neutral);
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
                    "<p>Commands and monitors e-foc motor controllers over CAN, up to %3 per bus.</p>"
                    "<p>CAN contract %4.%5 · Qt %6</p>"
                    "<p><a href=\"%7\">Project page</a> · <a href=\"%8\">Releases</a> · <a href=\"%9\">Report an issue</a></p>")
                    .arg(ToQt(application::displayName), ToQt(application::version))
                    .arg(maxMotors)
                    .arg(can::focContractVersionMajor)
                    .arg(can::focContractVersionMinor)
                    .arg(QString::fromUtf8(qVersion()), ToQt(application::repositoryUrl), ToQt(application::releasesUrl), ToQt(application::newIssueUrl)) +
                QString("<p>Log files: %1</p><p>Distributed under the e-foc Non-Commercial License.</p>").arg(QDir::toNativeSeparators(issueLog.Directory()).toHtmlEscaped()));
    }

    void MainWindow::OnConnectionChanged(bool connected)
    {
        busConnected = connected;
        connectionPanel->OnConnectionChanged(connected);
        updateSession.SetBusConnected(connected);

        if (connected)
        {
            Report(IssueSeverity::info, QString("Connected to %1 at %2").arg(connectionPanel->InterfaceName(), connectionPanel->BitrateText()));
            SetupSocketNotifier();
        }
        else
        {
            Report(IssueSeverity::info, "Disconnected");
            TeardownSocketNotifier();
        }

        for (const auto& [nodeId, motor] : motors)
            motor->SetBusConnected(connected);

        UpdateSafetyControls();
        UpdateStatusBar();
    }

    void MainWindow::OnNodeOnlineChanged(uint16_t nodeId, bool online)
    {
        auto* motor = Motor(nodeId);
        if (motor == nullptr && online)
        {
            if (motors.size() >= maxMotors)
            {
                Report(IssueSeverity::warning, QString("Node %1 is on the bus but not shown: the tool follows up to %2 motors").arg(nodeId).arg(maxMotors));
                return;
            }

            Report(IssueSeverity::info, QString("Discovered motor controller at node %1").arg(nodeId));
            AddMotor(nodeId);
            return;
        }

        if (motor != nullptr)
            motor->OnNodeOnlineChanged(online);
        UpdateSafetyControls();
        UpdateStatusBar();
    }

    void MainWindow::OnBusyChanged(uint16_t nodeId, bool busy)
    {
        if (auto* motor = Motor(nodeId))
            motor->OnBusyChanged(busy);
    }

    void MainWindow::OnCommandTimeout(uint16_t nodeId)
    {
        if (auto* motor = Motor(nodeId))
            motor->OnCommandTimeout();
        else
            Report(IssueSeverity::warning, QString("Node %1: no response to the last command").arg(nodeId));
    }

    void MainWindow::OnCommandAck(uint16_t nodeId, uint8_t categoryId, uint8_t commandType, services::CanAckStatus status)
    {
        if (auto* motor = Motor(nodeId))
            motor->OnCommandAck(categoryId, commandType, status);
    }

    void MainWindow::OnCommandRejected(uint16_t nodeId, uint8_t commandType, can::FocMotorCategoryError error)
    {
        if (auto* motor = Motor(nodeId))
            motor->OnCommandRejected(commandType, error);
    }

    void MainWindow::OnContractVersion(uint16_t nodeId, uint8_t major, uint8_t minor, bool compatible)
    {
        if (auto* motor = Motor(nodeId))
            motor->OnContractVersion(major, minor, compatible);
        UpdateSafetyControls();
    }

    void MainWindow::OnControlModeAcknowledged(uint16_t nodeId, can::FocMotorMode activeMode)
    {
        if (auto* motor = Motor(nodeId))
            motor->OnControlModeAcknowledged(activeMode);
    }

    void MainWindow::OnMotorStatusReceived(uint16_t nodeId, FocMotorState state, FocFaultCode fault)
    {
        if (auto* motor = Motor(nodeId))
            motor->OnMotorStatus(state, fault);
    }

    void MainWindow::OnCurrentMeasurementReceived(uint16_t nodeId, float idCurrent, float iqCurrent)
    {
        if (auto* motor = Motor(nodeId))
            motor->OnCurrentMeasurement(idCurrent, iqCurrent);
    }

    void MainWindow::OnSpeedPositionReceived(uint16_t nodeId, float speed, float position)
    {
        if (auto* motor = Motor(nodeId))
            motor->OnSpeedPosition(speed, position);
    }

    void MainWindow::OnBusVoltageReceived(uint16_t nodeId, float voltage)
    {
        if (auto* motor = Motor(nodeId))
            motor->OnBusVoltage(voltage);
    }

    void MainWindow::OnFaultEventReceived(uint16_t, FocFaultCode)
    {}

    void MainWindow::OnElectricalParametersReceived(uint16_t nodeId, float resistanceOhm, float inductanceMilliHenry, uint8_t polePairs)
    {
        if (auto* motor = Motor(nodeId))
            motor->OnElectricalParameters(resistanceOhm, inductanceMilliHenry, polePairs);
    }

    void MainWindow::OnMechanicalParametersReceived(uint16_t nodeId, float frictionNmSPerRad, float inertiaKgM2)
    {
        if (auto* motor = Motor(nodeId))
            motor->OnMechanicalParameters(frictionNmSPerRad, inertiaKgM2);
    }

    void MainWindow::OnAdapterError(infra::BoundedConstString message)
    {
        Report(IssueSeverity::error, QString("CAN adapter: %1").arg(QString::fromUtf8(message.data(), static_cast<qsizetype>(message.size()))));
    }

    void MainWindow::OnFrameLog(bool transmitted, uint32_t id, const services::CanFrame& frame)
    {
        eventLog->AppendFrame(transmitted, id, frame);
    }

    void MainWindow::SetupSocketNotifier()
    {
        TeardownSocketNotifier();

#if defined(__linux__)
        if (const auto fd = adapter.FileDescriptor(); fd >= 0)
        {
            socketNotifier = new QSocketNotifier(static_cast<qintptr>(fd), QSocketNotifier::Read, this);
            connect(socketNotifier, &QSocketNotifier::activated, this, [this]()
                {
                    adapter.ProcessReadEvent();
                });
            return;
        }
#endif

        rxPollTimer.start(1);
    }

    void MainWindow::TeardownSocketNotifier()
    {
        rxPollTimer.stop();
        delete socketNotifier;
        socketNotifier = nullptr;
    }
}
