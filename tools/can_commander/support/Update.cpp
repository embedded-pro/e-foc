#include "tools/can_commander/support/Update.hpp"
#include "tools/can_commander/support/ApplicationInfo.hpp"
#include "ui/backend/qt/QtConversions.hpp"

namespace tool
{
    QByteArray UserAgent()
    {
        return QByteArray{ "e-foc-can-commander/" }.append(application::version.data(), static_cast<qsizetype>(application::version.size()));
    }

    UpdateProduct CommanderProduct()
    {
        using ui::backend::qt::ToQt;
        return UpdateProduct{ ToQt(application::displayName), ToQt(application::name), ToQt(application::version), QUrl{ ToQt(application::latestReleaseApiUrl) },
            QUrl{ ToQt(application::releasesUrl) }, QUrl{ ToQt(application::newIssueUrl) }, UserAgent(), "Disconnect from the CAN bus" };
    }

    UpdateProduct VirtualMotorProduct()
    {
        auto product = CommanderProduct();
        product.displayName = "e-foc Virtual Motor";
        product.shortName = "Virtual Motor";
        return product;
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
