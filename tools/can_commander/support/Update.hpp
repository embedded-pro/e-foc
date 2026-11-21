#pragma once

#include "ui/backend/qt/update/IssueLog.hpp"
#include "ui/backend/qt/update/IssueReport.hpp"
#include "ui/backend/qt/update/ReleaseFeed.hpp"
#include "ui/backend/qt/update/ReleaseVersion.hpp"
#include "ui/backend/qt/update/UpdateChecker.hpp"
#include "ui/backend/qt/update/UpdateDownloader.hpp"
#include "ui/backend/qt/update/UpdateInstallation.hpp"
#include "ui/backend/qt/update/UpdateInstaller.hpp"
#include "ui/backend/qt/update/UpdateSession.hpp"
#include <QByteArray>

namespace tool
{
    using ui::backend::qt::update::BuildIssueUrl;
    using ui::backend::qt::update::CurrentInstallerPlatform;
    using ui::backend::qt::update::CurrentIssueEnvironment;
    using ui::backend::qt::update::DefaultUpdateCommand;
    using ui::backend::qt::update::DetectInstallation;
    using ui::backend::qt::update::EvaluateLatestRelease;
    using ui::backend::qt::update::InstallationKind;
    using ui::backend::qt::update::InstallCommand;
    using ui::backend::qt::update::InstallerAsset;
    using ui::backend::qt::update::InstallerPlatform;
    using ui::backend::qt::update::IssueEnvironment;
    using ui::backend::qt::update::IssueLog;
    using ui::backend::qt::update::IssueProduct;
    using ui::backend::qt::update::IssueSeverity;
    using ui::backend::qt::update::ParseLatestRelease;
    using ui::backend::qt::update::ReleaseInfo;
    using ui::backend::qt::update::ReleaseVersion;
    using ui::backend::qt::update::UpdateChecker;
    using ui::backend::qt::update::UpdateDownloader;
    using ui::backend::qt::update::UpdateInstaller;
    using ui::backend::qt::update::UpdatePhase;
    using ui::backend::qt::update::UpdateSession;
    using ui::backend::qt::update::UpdateState;
    using ui::backend::qt::update::UpdateStatus;

    QByteArray UserAgent();

    std::optional<InstallerAsset> FirmwarePackFor(const ReleaseInfo& release, const QString& boardName);
}
