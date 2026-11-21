#include "tools/can_commander/support/Update.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace tool;

    class TestFirmwarePack
        : public testing::Test
    {
    public:
        static QByteArray Release(const QString& tag, const QString& assets)
        {
            return QString(R"({
                "tag_name": "%1",
                "html_url": "https://github.com/embedded-pro/e-foc/releases/tag/%1",
                "assets": [%2]
            })")
                .arg(tag, assets)
                .toUtf8();
        }

        static QString Asset(const QString& name, const QString& digest = sha256Digest)
        {
            return QString(R"({ "name": "%1", "size": 4096, "digest": "%2", "browser_download_url": "https://github.com/embedded-pro/e-foc/releases/download/v1.1.0/%1" })").arg(name, digest);
        }

        static inline const QString sha256Hex{ "d02c96fe999b3b2bbab16cd745a0a94c269609a26269cc171764ebb48670a83e" };
        static inline const QString sha256Digest{ "sha256:" + sha256Hex };

        const QString assets{ Asset("e_foc-can_commander-v1.1.0-linux-x86_64.tar.gz") + "," +
                              Asset("e_foc-can_commander-v1.1.0-linux-x86_64.deb") + "," +
                              Asset("e_foc-can_commander-v1.1.0-windows-x86_64.zip") + "," +
                              Asset("e_foc-can_commander-v1.1.0-windows-x86_64-setup.exe") };
    };
}

TEST_F(TestFirmwarePack, finds_the_firmware_pack_of_a_board_by_its_asset_name)
{
    const auto release = ParseLatestRelease(Release("v1.7.0", assets + "," + Asset("e_foc-firmware-EK-TM4C1294XL-sync_foc_sensored-v1.7.0.upgrade") + "," + Asset("e_foc-firmware-EK-TM4C1294XL-sync_foc_sensored-v1.7.0.hex")), InstallerPlatform::linuxX64);

    ASSERT_TRUE(release.has_value());
    const auto pack = FirmwarePackFor(*release, "EK-TM4C1294XL");
    ASSERT_TRUE(pack.has_value());
    EXPECT_EQ("e_foc-firmware-EK-TM4C1294XL-sync_foc_sensored-v1.7.0.upgrade", pack->fileName);
    EXPECT_EQ(4096, pack->size);
    EXPECT_TRUE(pack->sha256.has_value());
}

TEST_F(TestFirmwarePack, a_board_without_a_pack_in_the_release_finds_none)
{
    const auto release = ParseLatestRelease(Release("v1.7.0", assets + "," + Asset("e_foc-firmware-EK-TM4C1294XL-sync_foc_sensored-v1.7.0.upgrade")), InstallerPlatform::linuxX64);

    ASSERT_TRUE(release.has_value());
    EXPECT_FALSE(FirmwarePackFor(*release, "NUCLEO-H563ZI").has_value());
}

TEST_F(TestFirmwarePack, a_pack_named_for_another_release_is_not_this_release_s_pack)
{
    const auto release = ParseLatestRelease(Release("v1.7.0", Asset("e_foc-firmware-EK-TM4C1294XL-sync_foc_sensored-v1.6.0.upgrade")), InstallerPlatform::linuxX64);

    ASSERT_TRUE(release.has_value());
    EXPECT_FALSE(FirmwarePackFor(*release, "EK-TM4C1294XL").has_value());
}

TEST_F(TestFirmwarePack, a_firmware_pack_not_downloadable_over_https_is_ignored)
{
    const auto insecure = QString(R"({ "name": "e_foc-firmware-EK-TM4C1294XL-sync_foc_sensored-v1.7.0.upgrade", "browser_download_url": "http://example.com/e_foc-firmware-EK-TM4C1294XL-sync_foc_sensored-v1.7.0.upgrade" })");
    const auto unlinked = QString(R"({ "name": "e_foc-firmware-NUCLEO-G474RE-sync_foc_sensored-v1.7.0.upgrade" })");
    const auto release = ParseLatestRelease(Release("v1.7.0", insecure + "," + unlinked), InstallerPlatform::linuxX64);

    ASSERT_TRUE(release.has_value());
    EXPECT_FALSE(FirmwarePackFor(*release, "EK-TM4C1294XL").has_value());
}
