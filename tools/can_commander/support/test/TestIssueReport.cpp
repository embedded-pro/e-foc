#include "tools/can_commander/support/IssueReport.hpp"
#include "gtest/gtest.h"
#include <QDir>
#include <QUrlQuery>

namespace
{
    using namespace tool;

    class TestIssueReport
        : public testing::Test
    {
    public:
        static QString Item(const QUrl& url, const QString& key)
        {
            return QUrlQuery{ url }.queryItemValue(key, QUrl::FullyDecoded);
        }

        const QUrl newIssue{ "https://github.com/embedded-pro/e-foc/issues/new" };
        const IssueEnvironment environment{ "1.2.3", "Ubuntu 24.04 LTS", "x86_64", "6.4.2", "~/.local/share/e-foc/CAN Commander/logs/can_commander.log" };
    };
}

TEST_F(TestIssueReport, opens_new_issue_page_with_prefixed_title)
{
    const auto url = BuildIssueUrl(newIssue, environment, {});

    EXPECT_EQ("https", url.scheme());
    EXPECT_EQ("github.com", url.host());
    EXPECT_EQ("/embedded-pro/e-foc/issues/new", url.path());
    EXPECT_EQ("[CAN Commander] ", Item(url, "title"));
}

TEST_F(TestIssueReport, body_lists_environment_and_log_file)
{
    const auto body = Item(BuildIssueUrl(newIssue, environment, {}), "body");

    EXPECT_TRUE(body.contains("| e-foc CAN Commander | 1.2.3 |"));
    EXPECT_TRUE(body.contains("| Operating system | Ubuntu 24.04 LTS |"));
    EXPECT_TRUE(body.contains("| Architecture | x86_64 |"));
    EXPECT_TRUE(body.contains("| Qt | 6.4.2 |"));
    EXPECT_TRUE(body.contains("~/.local/share/e-foc/CAN Commander/logs/can_commander.log"));
    EXPECT_TRUE(body.contains("(no entries)"));
}

TEST_F(TestIssueReport, log_lines_survive_reserved_characters)
{
    const QStringList log{ "duty 50% + offset & #3 = done?", "path C:\\temp\\a b" };
    const auto body = Item(BuildIssueUrl(newIssue, environment, log), "body");

    EXPECT_TRUE(body.contains("duty 50% + offset & #3 = done?\npath C:\\temp\\a b"));
}

TEST_F(TestIssueReport, drops_oldest_lines_to_fit_url_limit)
{
    QStringList log;
    for (int index = 0; index < 500; ++index)
        log.append(QString("2026-10-24T10:00:00.000Z [WARN ] timeout number %1").arg(index));

    const auto url = BuildIssueUrl(newIssue, environment, log, 3000);
    const auto body = Item(url, "body");

    EXPECT_LE(url.toEncoded().size(), 3000);
    EXPECT_TRUE(body.contains("timeout number 499"));
    EXPECT_FALSE(body.contains("timeout number 0\n"));
}

TEST_F(TestIssueReport, environment_hides_home_directory)
{
    const auto current = CurrentIssueEnvironment("1.2.3", QDir::homePath() + "/logs/can_commander.log");

    EXPECT_EQ("1.2.3", current.applicationVersion);
    EXPECT_TRUE(current.logFilePath.startsWith("~"));
    EXPECT_FALSE(current.logFilePath.contains(QDir::toNativeSeparators(QDir::homePath())));
    EXPECT_FALSE(current.qtVersion.isEmpty());
}
