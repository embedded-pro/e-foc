#include "tools/can_commander/support/Update.hpp"
#include "tools/can_commander/support/ApplicationInfo.hpp"

namespace tool
{
    QByteArray UserAgent()
    {
        return QByteArray{ "e-foc-can-commander/" }.append(application::version.data(), static_cast<qsizetype>(application::version.size()));
    }

    std::optional<InstallerAsset> FirmwarePackFor(const ReleaseInfo& release, const QString& boardName)
    {
        const auto name = QString("e_foc-firmware-%1-sync_foc_sensored-%2.upgrade").arg(boardName, release.tag);
        for (const auto& asset : release.assets)
            if (asset.fileName == name)
                return asset;
        return std::nullopt;
    }
}
