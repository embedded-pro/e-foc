#include "tools/can_commander/support/IssueLog.hpp"
#include "gtest/gtest.h"
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>

namespace
{
    using tool::IssueLog;
    using tool::IssueSeverity;

    class TestIssueLog
        : public testing::Test
    {
    public:
        IssueLog::Config Config() const
        {
            IssueLog::Config config;
            config.directory = QDir{ temporary.path() }.filePath("logs");
            return config;
        }

        static QStringList FileLines(const QString& path)
        {
            QFile file{ path };
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                return {};
            return QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
        }

        QTemporaryDir temporary;
    };
}

TEST_F(TestIssueLog, writes_utc_timestamped_line_with_severity)
{
    IssueLog log{ Config() };
    log.Write(IssueSeverity::warning, "Timeout: no response to Start Motor");

    const auto lines = FileLines(log.FilePath());
    ASSERT_EQ(1, lines.size());
    const QRegularExpression format{ R"(^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z \[WARN \] Timeout: no response to Start Motor$)" };
    EXPECT_TRUE(format.match(lines.front()).hasMatch()) << lines.front().toStdString();
}

TEST_F(TestIssueLog, creates_missing_log_directory)
{
    IssueLog log{ Config() };

    EXPECT_TRUE(log.IsWritable());
    EXPECT_TRUE(QDir{ Config().directory }.exists());
    EXPECT_EQ(QDir{ Config().directory }.filePath("can_commander.log"), log.FilePath());
}

TEST_F(TestIssueLog, keeps_each_entry_on_one_line)
{
    IssueLog log{ Config() };
    log.Write(IssueSeverity::error, "first\nsecond\r\nthird");

    const auto lines = FileLines(log.FilePath());
    ASSERT_EQ(1, lines.size());
    EXPECT_TRUE(lines.front().endsWith("[ERROR] first second third"));
}

TEST_F(TestIssueLog, appends_across_sessions)
{
    {
        IssueLog log{ Config() };
        log.Write(IssueSeverity::info, "session one");
    }
    IssueLog log{ Config() };
    log.Write(IssueSeverity::info, "session two");

    const auto lines = FileLines(log.FilePath());
    ASSERT_EQ(2, lines.size());
    EXPECT_TRUE(lines[0].endsWith("session one"));
    EXPECT_TRUE(lines[1].endsWith("session two"));
}

TEST_F(TestIssueLog, rotates_and_keeps_configured_number_of_files)
{
    auto config = Config();
    config.maxFileBytes = 256;
    config.keptFiles = 3;
    IssueLog log{ config };

    for (int index = 0; index < 100; ++index)
        log.Write(IssueSeverity::info, QString("entry %1").arg(index));

    const QDir directory{ config.directory };
    EXPECT_TRUE(directory.exists("can_commander.log"));
    EXPECT_TRUE(directory.exists("can_commander.1.log"));
    EXPECT_TRUE(directory.exists("can_commander.2.log"));
    EXPECT_FALSE(directory.exists("can_commander.3.log"));
    EXPECT_TRUE(FileLines(log.FilePath()).back().endsWith("entry 99"));

    for (const auto& name : directory.entryList(QDir::Files))
        EXPECT_LT(QFileInfo{ directory.filePath(name) }.size(), config.maxFileBytes + 100) << name.toStdString();
}

TEST_F(TestIssueLog, remembers_only_the_most_recent_lines)
{
    auto config = Config();
    config.recentCapacity = 3;
    IssueLog log{ config };

    for (int index = 0; index < 5; ++index)
        log.Write(IssueSeverity::info, QString("entry %1").arg(index));

    const auto all = log.RecentLines(10);
    ASSERT_EQ(3, all.size());
    EXPECT_TRUE(all[0].endsWith("entry 2"));
    EXPECT_TRUE(all[2].endsWith("entry 4"));

    const auto newest = log.RecentLines(2);
    ASSERT_EQ(2, newest.size());
    EXPECT_TRUE(newest[0].endsWith("entry 3"));
}

TEST_F(TestIssueLog, keeps_recent_lines_when_directory_is_not_writable)
{
    const auto blocker = QDir{ temporary.path() }.filePath("blocker");
    QFile file{ blocker };
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.close();

    auto config = Config();
    config.directory = QDir{ blocker }.filePath("logs");
    IssueLog log{ config };
    log.Write(IssueSeverity::error, "adapter lost");

    EXPECT_FALSE(log.IsWritable());
    ASSERT_EQ(1, log.RecentLines(10).size());
    EXPECT_TRUE(log.RecentLines(10).front().endsWith("adapter lost"));
}

TEST_F(TestIssueLog, captures_qt_warnings_while_alive)
{
    {
        IssueLog log{ Config() };
        log.CaptureQtMessages();
        qWarning("tls backend missing");

        const auto lines = log.RecentLines(10);
        ASSERT_EQ(1, lines.size());
        EXPECT_TRUE(lines.front().endsWith("[WARN ] Qt: tls backend missing"));
    }

    IssueLog later{ Config() };
    qWarning("after the first log is gone");
    EXPECT_TRUE(later.RecentLines(10).isEmpty());
}
