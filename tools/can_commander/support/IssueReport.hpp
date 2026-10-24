#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

namespace tool
{
    struct IssueEnvironment
    {
        QString applicationVersion;
        QString operatingSystem;
        QString cpuArchitecture;
        QString qtVersion;
        QString logFilePath;
    };

    IssueEnvironment CurrentIssueEnvironment(const QString& applicationVersion, const QString& logFilePath);

    // GitHub answers 414 to long query strings, so the newest log lines that keep the encoded URL
    // within maxUrlLength are kept and older ones dropped.
    QUrl BuildIssueUrl(const QUrl& newIssueUrl, const IssueEnvironment& environment, const QStringList& recentLog, int maxUrlLength = 7000);
}
