#pragma once

#include "tools/can_commander/support/ReleaseVersion.hpp"
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <optional>

namespace tool
{
    enum class InstallerPlatform
    {
        windowsX64,
        linuxX64,
        unsupported
    };

    struct InstallerAsset
    {
        QString fileName;
        QUrl url;
        qint64 size{ 0 };
        std::optional<QByteArray> sha256;
    };

    struct ReleaseInfo
    {
        ReleaseVersion version;
        QString tag;
        QUrl pageUrl;
        std::optional<InstallerAsset> installer;
    };

    enum class UpdateStatus
    {
        updateAvailable,
        upToDate,
        invalidResponse
    };

    struct UpdateEvaluation
    {
        UpdateStatus status{ UpdateStatus::invalidResponse };
        std::optional<ReleaseInfo> latest;
    };

    InstallerPlatform CurrentInstallerPlatform();
    QString InstallerAssetSuffix(InstallerPlatform platform);

    std::optional<QByteArray> ParseSha256Digest(const QString& digest);
    std::optional<ReleaseInfo> ParseLatestRelease(const QByteArray& json, InstallerPlatform platform);
    UpdateEvaluation EvaluateLatestRelease(const QByteArray& json, const ReleaseVersion& current, InstallerPlatform platform);
}
