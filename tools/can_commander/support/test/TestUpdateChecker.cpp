#include "tools/can_commander/support/UpdateChecker.hpp"
#include "gtest/gtest.h"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>

namespace
{
    using namespace tool;

    class TestUpdateChecker
        : public testing::Test
    {
    public:
        static void SetUpTestSuite()
        {
            static int argc{ 1 };
            static char name[]{ "support_test" };
            static char* argv[]{ name, nullptr };

            if (QCoreApplication::instance() == nullptr)
                application = std::make_unique<QCoreApplication>(argc, argv);
        }

        static void TearDownTestSuite()
        {
            application.reset();
        }

        QUrl Feed(const QByteArray& content)
        {
            const auto path = QDir{ temporary.path() }.filePath("latest.json");
            QFile file{ path };
            EXPECT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(content);
            return QUrl::fromLocalFile(path);
        }

        static QByteArray Release(const QString& tag)
        {
            return QString(R"({ "tag_name": "%1", "html_url": "https://github.com/embedded-pro/e-foc/releases/tag/%1", "assets": [] })").arg(tag).toUtf8();
        }

        void Check(const QUrl& feed, int checks = 1)
        {
            UpdateChecker checker{ ReleaseVersion{ 1, 0, 0 }, feed };
            QEventLoop loop;
            QObject::connect(&checker, &UpdateChecker::UpdateAvailable, &loop, [this, &loop](const ReleaseInfo& release)
                {
                    ++emitted;
                    available = release;
                    loop.quit();
                });
            QObject::connect(&checker, &UpdateChecker::UpToDate, &loop, [this, &loop](const ReleaseInfo& release)
                {
                    ++emitted;
                    upToDate = release;
                    loop.quit();
                });
            QObject::connect(&checker, &UpdateChecker::CheckFailed, &loop, [this, &loop](const QString& reason)
                {
                    ++emitted;
                    failure = reason;
                    loop.quit();
                });
            QTimer::singleShot(5000, &loop, &QEventLoop::quit);

            for (int index = 0; index < checks; ++index)
                checker.Check();
            EXPECT_TRUE(checker.IsChecking());

            loop.exec();
            QCoreApplication::processEvents();
            EXPECT_FALSE(checker.IsChecking());
        }

        static inline std::unique_ptr<QCoreApplication> application;
        QTemporaryDir temporary;
        int emitted{ 0 };
        std::optional<ReleaseInfo> available;
        std::optional<ReleaseInfo> upToDate;
        std::optional<QString> failure;
    };
}

TEST_F(TestUpdateChecker, announces_newer_release)
{
    Check(Feed(Release("v1.4.0")));

    ASSERT_TRUE(available.has_value());
    EXPECT_EQ((ReleaseVersion{ 1, 4, 0 }), available->version);
    EXPECT_FALSE(upToDate.has_value());
    EXPECT_FALSE(failure.has_value());
}

TEST_F(TestUpdateChecker, reports_up_to_date_for_current_release)
{
    Check(Feed(Release("v1.0.0")));

    ASSERT_TRUE(upToDate.has_value());
    EXPECT_FALSE(available.has_value());
}

TEST_F(TestUpdateChecker, reports_failure_for_unreachable_feed)
{
    Check(QUrl::fromLocalFile(QDir{ temporary.path() }.filePath("missing.json")));

    ASSERT_TRUE(failure.has_value());
    EXPECT_FALSE(failure->isEmpty());
}

TEST_F(TestUpdateChecker, reports_failure_for_malformed_feed)
{
    Check(Feed(R"({ "message": "Not Found" })"));

    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ("the release feed did not contain a valid release", *failure);
}

TEST_F(TestUpdateChecker, ignores_check_while_one_is_pending)
{
    Check(Feed(Release("v2.0.0")), 3);
    QCoreApplication::processEvents();

    EXPECT_EQ(1, emitted);
}
