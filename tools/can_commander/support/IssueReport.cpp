#include "tools/can_commander/support/IssueReport.hpp"
#include <QDir>
#include <QSysInfo>
#include <QUrlQuery>

namespace tool
{
    namespace
    {
        QString IssueBody(const IssueEnvironment& environment, const QStringList& logLines)
        {
            auto body = QString("### What happened?\n\n"
                                "<!-- What did you do, what did you expect, and what happened instead? -->\n\n"
                                "### Environment\n\n"
                                "| | |\n|---|---|\n"
                                "| e-foc CAN Commander | %1 |\n"
                                "| Operating system | %2 |\n"
                                "| Architecture | %3 |\n"
                                "| Qt | %4 |\n\n")
                            .arg(environment.applicationVersion, environment.operatingSystem, environment.cpuArchitecture, environment.qtVersion);

            body += "### Recent log\n\n```text\n";
            body += logLines.isEmpty() ? QString("(no entries)") : logLines.join('\n');
            body += "\n```\n";

            if (!environment.logFilePath.isEmpty())
                body += QString("\nFull log: `%1` — attach it if more context helps.\n").arg(environment.logFilePath);

            return body;
        }

        // QUrlQuery reads '%' as the start of an escape and leaves '+' alone, which GitHub decodes as a
        // space; encoding every reserved character up front keeps log text such as "50%" or "a+b" intact.
        QString Encoded(const QString& text)
        {
            return QString::fromLatin1(QUrl::toPercentEncoding(text));
        }

        QUrl IssueUrl(const QUrl& newIssueUrl, const IssueEnvironment& environment, const QStringList& logLines)
        {
            QUrlQuery query;
            query.addQueryItem("title", Encoded("[CAN Commander] "));
            query.addQueryItem("body", Encoded(IssueBody(environment, logLines)));

            QUrl url{ newIssueUrl };
            url.setQuery(query);
            return url;
        }
    }

    IssueEnvironment CurrentIssueEnvironment(const QString& applicationVersion, const QString& logFilePath)
    {
        auto shownPath = QDir::toNativeSeparators(logFilePath);
        const auto home = QDir::toNativeSeparators(QDir::homePath());
        if (!home.isEmpty() && shownPath.startsWith(home))
            shownPath.replace(0, home.size(), "~");

        return IssueEnvironment{ applicationVersion, QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(), QString::fromUtf8(qVersion()), shownPath };
    }

    QUrl BuildIssueUrl(const QUrl& newIssueUrl, const IssueEnvironment& environment, const QStringList& recentLog, int maxUrlLength)
    {
        auto lines = recentLog;
        auto url = IssueUrl(newIssueUrl, environment, lines);

        while (!lines.isEmpty() && url.toEncoded().size() > maxUrlLength)
        {
            lines.removeFirst();
            url = IssueUrl(newIssueUrl, environment, lines);
        }

        return url;
    }
}
