#include "hal/generic/TimerServiceGeneric.hpp"
#include "tools/can_commander/gui/MainWindow.hpp"
#include "tools/can_commander/support/ApplicationInfo.hpp"
#include "tools/can_commander/support/IssueLog.hpp"
#include "tools/can_commander/support/QtEventDispatcher.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include "ui/theme/Theme.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QNetworkProxyFactory>
#include <QStandardPaths>
#include <QStringList>
#include <QSysInfo>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <optional>

#if defined(__linux__) || defined(_WIN32)
#include "can-lite/drivers/implementation/PlatformCanAdapter.hpp"
#endif

#ifdef _WIN32
#include <cstdio>
#include <windows.h>
#endif

namespace
{
    using ui::backend::qt::ToQt;

    bool HasArgument(int argc, char* argv[], std::initializer_list<const char*> names)
    {
        for (int index = 1; index < argc; ++index)
            for (const auto* name : names)
                if (std::strcmp(argv[index], name) == 0)
                    return true;

        return false;
    }

    void DescribeOptions(QCommandLineParser& parser, const QCommandLineOption& noUpdateCheck)
    {
        parser.setApplicationDescription("Commands and monitors e-foc motor controllers over CAN.");
        parser.addHelpOption();
        parser.addVersionOption();
        parser.addOption(noUpdateCheck);
    }

    void AttachToParentConsole()
    {
#if defined(_WIN32)
        const auto output = GetStdHandle(STD_OUTPUT_HANDLE);
        if ((output == nullptr || output == INVALID_HANDLE_VALUE) && AttachConsole(ATTACH_PARENT_PROCESS))
        {
            FILE* stream{ nullptr };
            freopen_s(&stream, "CONOUT$", "w", stdout);
            freopen_s(&stream, "CONOUT$", "w", stderr);
        }
#endif
    }

    bool AnswersWithoutGui(int argc, char* argv[], const QCommandLineOption& noUpdateCheck)
    {
        if (HasArgument(argc, argv, { "-v", "--version", "-h", "--help", "--help-all", "-?" }))
            AttachToParentConsole();

        if (HasArgument(argc, argv, { "-v", "--version" }))
        {
            std::cout << tool::application::displayName << " " << tool::application::version << "\n";
            return true;
        }

        if (!HasArgument(argc, argv, { "-h", "--help", "--help-all", "-?" }))
            return false;

        QCoreApplication core{ argc, argv };
        QCommandLineParser parser;
        DescribeOptions(parser, noUpdateCheck);
        parser.process(core);
        return true;
    }

    void ConfigureApplication()
    {
        QApplication::setOrganizationName(ToQt(tool::application::organization));
        QApplication::setApplicationName(ToQt(tool::application::name));
        QApplication::setApplicationDisplayName(ToQt(tool::application::displayName));
        QApplication::setApplicationVersion(ToQt(tool::application::version));
        QApplication::setWindowIcon(QIcon{ ":/icons/e-foc-can-commander.png" });
#if defined(__linux__)
        QGuiApplication::setDesktopFileName("e-foc-can-commander");
#endif
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        ui::backend::qt::ApplyTheme(ui::theme::Instrument());
    }

    tool::IssueLog::Config IssueLogConfig()
    {
        tool::IssueLog::Config config;
        config.directory = QDir{ QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) }.filePath("logs");
        return config;
    }

    std::optional<QString> MissingDrivers(const services::CanBusAdapter& adapter)
    {
        QStringList reasons;
        adapter.EnumerateMissingDrivers([&reasons](infra::BoundedConstString reason)
            {
                reasons.append(QString::fromUtf8(reason.data(), static_cast<qsizetype>(reason.size())));
            });

        if (reasons.isEmpty())
            return std::nullopt;
        return "No CAN adapter is ready. Plug one in, or install the driver it needs:\n\n" + reasons.join("\n\n");
    }
}

int main(int argc, char* argv[])
{
    const QCommandLineOption noUpdateCheck{ "no-update-check", "Do not look for a newer release at startup." };
    if (AnswersWithoutGui(argc, argv, noUpdateCheck))
        return 0;

#if defined(__linux__)
    if (QFile::exists("/.dockerenv") && qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("DISPLAY", "host.docker.internal:0.0");
#endif

    QApplication app(argc, argv);
    ConfigureApplication();

    QCommandLineParser parser;
    DescribeOptions(parser, noUpdateCheck);
    parser.process(app);

    tool::IssueLog issueLog{ IssueLogConfig() };
    issueLog.CaptureQtMessages();
    issueLog.Write(tool::IssueSeverity::info, QString("Session started: %1 %2 on %3 (%4), Qt %5")
                                                  .arg(ToQt(tool::application::displayName), ToQt(tool::application::version), QSysInfo::prettyProductName(),
                                                      QSysInfo::currentCpuArchitecture(), QString::fromUtf8(qVersion())));

    tool::QtEventDispatcher::WithSize<256> eventDispatcher;
    hal::TimerServiceGeneric timerService;

#if defined(__linux__) || defined(_WIN32)
    services::PlatformCanAdapter platformAdapter;
    auto& adapter = platformAdapter.Adapter();
#else
    std::cerr << "No CAN adapter available for this platform.\n"
              << "Supported platforms: Linux (SocketCAN, slcan), Windows (PCAN, Kvaser, slcan, candleLight)\n";
    return 1;
#endif

#if defined(__linux__) || defined(_WIN32)
    tool::MainWindow window(adapter, issueLog);
    window.show();

    if (!parser.isSet(noUpdateCheck))
        window.CheckForUpdatesOnStartup();

    if (const auto problem = MissingDrivers(adapter))
        window.ReportStartupProblem("No Usable CAN Adapter", *problem);

    return QApplication::exec();
#endif
}
