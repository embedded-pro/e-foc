#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

namespace tool
{
    struct IssueProduct
    {
        QString displayName;
        QString tag;
    };

    struct IssueEnvironment
    {
        IssueProduct product;
        QString applicationVersion;
        QString operatingSystem;
        QString cpuArchitecture;
        QString qtVersion;
        QString logFilePath;
    };

    IssueEnvironment CurrentIssueEnvironment(const IssueProduct& product, const QString& applicationVersion, const QString& logFilePath);

    QUrl BuildIssueUrl(const QUrl& newIssueUrl, const IssueEnvironment& environment, const QStringList& recentLog, int maxUrlLength = 7000);
}
