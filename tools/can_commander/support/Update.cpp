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
        UpdateProduct product;
        product.displayName = ToQt(application::displayName);
        product.shortName = ToQt(application::name);
        product.version = ToQt(application::version);
        product.feedUrl = QUrl{ ToQt(application::latestReleaseApiUrl) };
        product.releasesUrl = QUrl{ ToQt(application::releasesUrl) };
        product.newIssueUrl = QUrl{ ToQt(application::newIssueUrl) };
        product.userAgent = UserAgent();
        product.blockedAction = "Disconnect from the CAN bus";
        return product;
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
