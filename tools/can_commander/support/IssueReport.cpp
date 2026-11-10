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
                                "| %1 | %2 |\n"
                                "| Operating system | %3 |\n"
                                "| Architecture | %4 |\n"
                                "| Qt | %5 |\n\n")
                            .arg(environment.product.displayName, environment.applicationVersion, environment.operatingSystem, environment.cpuArchitecture, environment.qtVersion);

            body += "### Recent log\n\n```text\n";
            body += logLines.isEmpty() ? QString("(no entries)") : logLines.join('\n');
            body += "\n```\n";

            if (!environment.logFilePath.isEmpty())
                body += QString("\nFull log: `%1` — attach it if more context helps.\n").arg(environment.logFilePath);

            return body;
        }

        QString Encoded(const QString& text)
        {
            return QString::fromLatin1(QUrl::toPercentEncoding(text));
        }

        QUrl IssueUrl(const QUrl& newIssueUrl, const IssueEnvironment& environment, const QStringList& logLines)
        {
            QUrlQuery query;
            query.addQueryItem("title", Encoded(QString("[%1] ").arg(environment.product.tag)));
            query.addQueryItem("body", Encoded(IssueBody(environment, logLines)));

            QUrl url{ newIssueUrl };
            url.setQuery(query);
            return url;
        }
    }

    IssueEnvironment CurrentIssueEnvironment(const IssueProduct& product, const QString& applicationVersion, const QString& logFilePath)
    {
        auto shownPath = QDir::toNativeSeparators(logFilePath);
        const auto home = QDir::toNativeSeparators(QDir::homePath());
        if (!home.isEmpty() && shownPath.startsWith(home))
            shownPath.replace(0, home.size(), "~");

        return IssueEnvironment{ product, applicationVersion, QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(), QString::fromUtf8(qVersion()), shownPath };
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
