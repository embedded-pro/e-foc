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

    struct ReleaseInfo
    {
        ReleaseVersion version;
        QString tag;
        QUrl pageUrl;
        std::optional<QUrl> installerUrl;
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

    std::optional<ReleaseInfo> ParseLatestRelease(const QByteArray& json, InstallerPlatform platform);
    UpdateEvaluation EvaluateLatestRelease(const QByteArray& json, const ReleaseVersion& current, InstallerPlatform platform);
}
