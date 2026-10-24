#include "tools/can_commander/support/ReleaseFeed.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace tool;

    class TestReleaseFeed
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

TEST_F(TestReleaseFeed, reads_version_tag_and_release_page)
{
    const auto release = ParseLatestRelease(Release("v1.1.0", assets), InstallerPlatform::linuxX64);

    ASSERT_TRUE(release.has_value());
    EXPECT_EQ((ReleaseVersion{ 1, 1, 0 }), release->version);
    EXPECT_EQ("v1.1.0", release->tag);
    EXPECT_EQ(QUrl("https://github.com/embedded-pro/e-foc/releases/tag/v1.1.0"), release->pageUrl);
}

TEST_F(TestReleaseFeed, picks_debian_package_on_linux)
{
    const auto release = ParseLatestRelease(Release("v1.1.0", assets), InstallerPlatform::linuxX64);

    ASSERT_TRUE(release.has_value());
    ASSERT_TRUE(release->installer.has_value());
    EXPECT_EQ("e_foc-can_commander-v1.1.0-linux-x86_64.deb", release->installer->fileName);
    EXPECT_TRUE(release->installer->url.path().endsWith("e_foc-can_commander-v1.1.0-linux-x86_64.deb"));
}

TEST_F(TestReleaseFeed, picks_setup_executable_on_windows)
{
    const auto release = ParseLatestRelease(Release("v1.1.0", assets), InstallerPlatform::windowsX64);

    ASSERT_TRUE(release.has_value());
    ASSERT_TRUE(release->installer.has_value());
    EXPECT_TRUE(release->installer->url.path().endsWith("e_foc-can_commander-v1.1.0-windows-x86_64-setup.exe"));
}

TEST_F(TestReleaseFeed, has_no_installer_when_release_lacks_one)
{
    const auto release = ParseLatestRelease(Release("v1.1.0", Asset("e_foc-simulator-v1.1.0-linux-x86_64.tar.gz")), InstallerPlatform::linuxX64);

    ASSERT_TRUE(release.has_value());
    EXPECT_FALSE(release->installer.has_value());
}

TEST_F(TestReleaseFeed, has_no_installer_on_unsupported_platform)
{
    const auto release = ParseLatestRelease(Release("v1.1.0", assets), InstallerPlatform::unsupported);

    ASSERT_TRUE(release.has_value());
    EXPECT_FALSE(release->installer.has_value());
}

TEST_F(TestReleaseFeed, ignores_installer_not_served_over_https)
{
    const auto insecure = QString(R"({ "name": "e_foc-can_commander-v1.1.0-linux-x86_64.deb", "browser_download_url": "http://example.com/e_foc-can_commander-v1.1.0-linux-x86_64.deb" })");
    const auto release = ParseLatestRelease(Release("v1.1.0", insecure), InstallerPlatform::linuxX64);

    ASSERT_TRUE(release.has_value());
    EXPECT_FALSE(release->installer.has_value());
}

TEST_F(TestReleaseFeed, reads_installer_size_and_sha256_digest)
{
    const auto release = ParseLatestRelease(Release("v1.1.0", assets), InstallerPlatform::linuxX64);

    ASSERT_TRUE(release.has_value());
    ASSERT_TRUE(release->installer.has_value());
    EXPECT_EQ(4096, release->installer->size);
    ASSERT_TRUE(release->installer->sha256.has_value());
    EXPECT_EQ(QByteArray::fromHex(sha256Hex.toLatin1()), *release->installer->sha256);
    EXPECT_EQ(32, release->installer->sha256->size());
}

TEST_F(TestReleaseFeed, installer_without_a_usable_digest_has_none)
{
    for (const auto* digest : { "", "md5:0123456789abcdef0123456789abcdef", "sha256:", "sha256:d02c96fe", "sha256:z02c96fe999b3b2bbab16cd745a0a94c269609a26269cc171764ebb48670a83e" })
    {
        const auto release = ParseLatestRelease(Release("v1.1.0", Asset("e_foc-can_commander-v1.1.0-linux-x86_64.deb", digest)), InstallerPlatform::linuxX64);

        ASSERT_TRUE(release.has_value()) << digest;
        ASSERT_TRUE(release->installer.has_value()) << digest;
        EXPECT_FALSE(release->installer->sha256.has_value()) << digest;
    }
}

TEST_F(TestReleaseFeed, sha256_digest_accepts_upper_case_hex)
{
    const auto digest = ParseSha256Digest("sha256:" + sha256Hex.toUpper());

    ASSERT_TRUE(digest.has_value());
    EXPECT_EQ(QByteArray::fromHex(sha256Hex.toLatin1()), *digest);
}

TEST_F(TestReleaseFeed, rejects_documents_that_are_not_a_release)
{
    EXPECT_FALSE(ParseLatestRelease("", InstallerPlatform::linuxX64).has_value());
    EXPECT_FALSE(ParseLatestRelease("[]", InstallerPlatform::linuxX64).has_value());
    EXPECT_FALSE(ParseLatestRelease(R"({ "message": "API rate limit exceeded" })", InstallerPlatform::linuxX64).has_value());
    EXPECT_FALSE(ParseLatestRelease(Release("dry-run-12", assets), InstallerPlatform::linuxX64).has_value());
}

TEST_F(TestReleaseFeed, newer_release_is_an_update)
{
    const auto evaluation = EvaluateLatestRelease(Release("v1.1.0", assets), ReleaseVersion{ 1, 0, 9 }, InstallerPlatform::linuxX64);

    EXPECT_EQ(UpdateStatus::updateAvailable, evaluation.status);
    ASSERT_TRUE(evaluation.latest.has_value());
    EXPECT_EQ((ReleaseVersion{ 1, 1, 0 }), evaluation.latest->version);
}

TEST_F(TestReleaseFeed, same_or_older_release_is_up_to_date)
{
    EXPECT_EQ(UpdateStatus::upToDate, EvaluateLatestRelease(Release("v1.1.0", assets), ReleaseVersion{ 1, 1, 0 }, InstallerPlatform::linuxX64).status);
    EXPECT_EQ(UpdateStatus::upToDate, EvaluateLatestRelease(Release("v1.1.0", assets), ReleaseVersion{ 2, 0, 0 }, InstallerPlatform::linuxX64).status);
}

TEST_F(TestReleaseFeed, unreadable_feed_is_invalid)
{
    const auto evaluation = EvaluateLatestRelease("not json", ReleaseVersion{ 1, 0, 0 }, InstallerPlatform::linuxX64);

    EXPECT_EQ(UpdateStatus::invalidResponse, evaluation.status);
    EXPECT_FALSE(evaluation.latest.has_value());
}
