#include "tools/can_commander/support/ReleaseFeed.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace tool
{
    namespace
    {
        constexpr qsizetype sha256Bytes{ 32 };

        std::optional<InstallerAsset> DownloadableAsset(const QJsonValue& asset)
        {
            const auto object = asset.toObject();
            const QUrl url{ object.value("browser_download_url").toString() };
            if (!url.isValid() || url.scheme() != "https")
                return std::nullopt;

            return InstallerAsset{ object.value("name").toString(), url, object.value("size").toInteger(), ParseSha256Digest(object.value("digest").toString()) };
        }

        std::optional<InstallerAsset> FindInstaller(const QJsonArray& assets, InstallerPlatform platform)
        {
            const auto suffix = InstallerAssetSuffix(platform);
            if (suffix.isEmpty())
                return std::nullopt;

            for (const auto& asset : assets)
                if (auto installer = DownloadableAsset(asset); installer && installer->fileName.endsWith(suffix))
                    return installer;

            return std::nullopt;
        }

        std::vector<InstallerAsset> FindFirmwarePacks(const QJsonArray& assets)
        {
            std::vector<InstallerAsset> packs;
            for (const auto& asset : assets)
                if (auto pack = DownloadableAsset(asset); pack && pack->fileName.startsWith(QLatin1String("e_foc-firmware-")) && pack->fileName.endsWith(QLatin1String(".upgrade")))
                    packs.push_back(*pack);

            return packs;
        }
    }

    InstallerPlatform CurrentInstallerPlatform()
    {
#if defined(_WIN32)
        return InstallerPlatform::windowsX64;
#elif defined(__linux__)
        return InstallerPlatform::linuxX64;
#else
        return InstallerPlatform::unsupported;
#endif
    }

    QString InstallerAssetSuffix(InstallerPlatform platform)
    {
        switch (platform)
        {
            case InstallerPlatform::windowsX64:
                return "-windows-x86_64-setup.exe";
            case InstallerPlatform::linuxX64:
                return "-linux-x86_64.deb";
            default:
                return {};
        }
    }

    std::optional<QByteArray> ParseSha256Digest(const QString& digest)
    {
        static const QString prefix{ "sha256:" };
        if (!digest.startsWith(prefix))
            return std::nullopt;

        const auto hex = digest.mid(prefix.size());
        if (hex.size() != sha256Bytes * 2)
            return std::nullopt;

        for (const auto character : hex)
            if (!character.isDigit() && !QStringLiteral("abcdefABCDEF").contains(character))
                return std::nullopt;

        return QByteArray::fromHex(hex.toLatin1());
    }

    std::optional<ReleaseInfo> ParseLatestRelease(const QByteArray& json, InstallerPlatform platform)
    {
        const auto document = QJsonDocument::fromJson(json);
        if (!document.isObject())
            return std::nullopt;

        const auto root = document.object();
        const auto tag = root.value("tag_name").toString();
        const auto version = ReleaseVersion::Parse(tag.toStdString());
        const QUrl pageUrl{ root.value("html_url").toString() };

        if (!version || !pageUrl.isValid() || pageUrl.scheme() != "https")
            return std::nullopt;

        const auto assets = root.value("assets").toArray();
        return ReleaseInfo{ *version, tag, pageUrl, FindInstaller(assets, platform), FindFirmwarePacks(assets) };
    }

    std::optional<InstallerAsset> FirmwarePackFor(const ReleaseInfo& release, const QString& boardName)
    {
        const auto name = QString("e_foc-firmware-%1-sync_foc_sensored-%2.upgrade").arg(boardName, release.tag);
        for (const auto& pack : release.firmwarePacks)
            if (pack.fileName == name)
                return pack;
        return std::nullopt;
    }

    UpdateEvaluation EvaluateLatestRelease(const QByteArray& json, const ReleaseVersion& current, InstallerPlatform platform)
    {
        auto latest = ParseLatestRelease(json, platform);
        if (!latest)
            return {};

        const auto status = latest->version > current ? UpdateStatus::updateAvailable : UpdateStatus::upToDate;
        return UpdateEvaluation{ status, std::move(latest) };
    }
}
