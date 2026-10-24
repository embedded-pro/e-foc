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

        static QString Asset(const QString& name)
        {
            return QString(R"({ "name": "%1", "browser_download_url": "https://github.com/embedded-pro/e-foc/releases/download/v1.1.0/%1" })").arg(name);
        }

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
    ASSERT_TRUE(release->installerUrl.has_value());
    EXPECT_TRUE(release->installerUrl->path().endsWith("e_foc-can_commander-v1.1.0-linux-x86_64.deb"));
}

TEST_F(TestReleaseFeed, picks_setup_executable_on_windows)
{
    const auto release = ParseLatestRelease(Release("v1.1.0", assets), InstallerPlatform::windowsX64);

    ASSERT_TRUE(release.has_value());
    ASSERT_TRUE(release->installerUrl.has_value());
    EXPECT_TRUE(release->installerUrl->path().endsWith("e_foc-can_commander-v1.1.0-windows-x86_64-setup.exe"));
}

TEST_F(TestReleaseFeed, has_no_installer_when_release_lacks_one)
{
    const auto release = ParseLatestRelease(Release("v1.1.0", Asset("e_foc-simulator-v1.1.0-linux-x86_64.tar.gz")), InstallerPlatform::linuxX64);

    ASSERT_TRUE(release.has_value());
    EXPECT_FALSE(release->installerUrl.has_value());
}

TEST_F(TestReleaseFeed, has_no_installer_on_unsupported_platform)
{
    const auto release = ParseLatestRelease(Release("v1.1.0", assets), InstallerPlatform::unsupported);

    ASSERT_TRUE(release.has_value());
    EXPECT_FALSE(release->installerUrl.has_value());
}

TEST_F(TestReleaseFeed, ignores_installer_not_served_over_https)
{
    const auto insecure = QString(R"({ "name": "e_foc-can_commander-v1.1.0-linux-x86_64.deb", "browser_download_url": "http://example.com/e_foc-can_commander-v1.1.0-linux-x86_64.deb" })");
    const auto release = ParseLatestRelease(Release("v1.1.0", insecure), InstallerPlatform::linuxX64);

    ASSERT_TRUE(release.has_value());
    EXPECT_FALSE(release->installerUrl.has_value());
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
