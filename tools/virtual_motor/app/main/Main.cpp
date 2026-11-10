#include "can-lite/drivers/hub/VirtualCanHub.hpp"
#include "hal/generic/TimerServiceGeneric.hpp"
#include "tools/can_commander/support/ApplicationInfo.hpp"
#include "tools/can_commander/support/IssueLog.hpp"
#include "tools/can_commander/support/QtEventDispatcher.hpp"
#include "tools/virtual_motor/app/gui/ManagerWindow.hpp"
#include "tools/virtual_motor/app/gui/QtEngineProcesses.hpp"
#include "tools/virtual_motor/app/gui/QtRigStore.hpp"
#include "tools/virtual_motor/app/logic/Rig.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>

namespace
{
    QString Executable(const QString& name)
    {
#if defined(_WIN32)
        return name + ".exe";
#else
        return name;
#endif
    }

    QString Installed(const QString& name, const QString& builtAt)
    {
        const auto beside = QDir{ QCoreApplication::applicationDirPath() }.filePath(Executable(name));
        if (QFileInfo::exists(beside))
            return beside;
        if (QFileInfo::exists(builtAt))
            return builtAt;
        return {};
    }

    tool::IssueLog::Config IssueLogConfig()
    {
        tool::IssueLog::Config config;
        config.directory = QDir{ QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) }.filePath("logs");
        config.baseName = "virtual_motor";
        return config;
    }

    void Fail(tool::IssueLog& issueLog, QWidget& window, const QString& title, const QString& text)
    {
        issueLog.Write(tool::IssueSeverity::error, text);
        QMessageBox::critical(&window, title, text);
    }

    QString HubStatus(services::VirtualCanHub& hub)
    {
        if (hub.Listen())
            return QString("● hub running on port %1").arg(services::loopback::virtualCanHubPort);
        return QString("joined the hub already running on port %1").arg(services::loopback::virtualCanHubPort);
    }
}

int main(int argc, char* argv[])
{
    QApplication app{ argc, argv };
    QApplication::setOrganizationName("embedded-pro");
    QApplication::setApplicationName("e-foc Virtual Motor");
    QApplication::setWindowIcon(QIcon{ ":/icons/e-foc-virtual-motor.png" });
    ui::backend::qt::ApplyTheme(ui::theme::Instrument());

    tool::IssueLog issueLog{ IssueLogConfig() };
    issueLog.CaptureQtMessages();
    issueLog.Write(tool::IssueSeverity::info, QString("Session started: e-foc Virtual Motor %1 on %2 (%3), Qt %4")
                                                  .arg(QString::fromUtf8(tool::application::version.data(), static_cast<qsizetype>(tool::application::version.size())), QSysInfo::prettyProductName(),
                                                      QSysInfo::currentCpuArchitecture(), QString::fromUtf8(qVersion())));

    tool::QtEventDispatcher::WithSize<256> eventDispatcher;
    hal::TimerServiceGeneric timerService;
    services::VirtualCanHub hub;
    const auto hubStatus = HubStatus(hub);

    const auto engine = Installed("e-foc-virtual-motor", E_FOC_VIRTUAL_MOTOR_ENGINE);
    const auto folder = QDir{ QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) }.filePath("motors");
    virtual_motor::app::QtEngineProcesses processes{ engine };
    virtual_motor::app::QtRigStore store;
    virtual_motor::app::Rig rig{ processes, store, folder.toStdString(), services::loopback::virtualCanHubPort };
    processes.onEnded = [&rig](virtual_motor::app::ProcessId process)
    {
        rig.ProcessEnded(process);
    };
    processes.onOutput = [&rig](virtual_motor::app::ProcessId process, const std::string& line)
    {
        rig.ProcessOutput(process, line);
    };

    virtual_motor::app::ManagerWindow window{ rig, issueLog, hubStatus, Installed("e-foc-can-commander", E_FOC_CAN_COMMANDER) };
    rig.Observe(window);
    window.show();
    if (engine.isEmpty())
        Fail(issueLog, window, "Virtual Motor Program Missing", "e-foc-virtual-motor was not found next to this app; no motor can start. Reinstall the e-foc tools.");
    if (!rig.Open())
        Fail(issueLog, window, "Plant Port Unavailable", "The app could not open its plant port on this machine's loopback interface; motors cannot be watched.");

    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, [&rig, &hub]()
        {
            hub.Poll();
            rig.Poll();
        });
    poll.start(2);
    return QApplication::exec();
}
