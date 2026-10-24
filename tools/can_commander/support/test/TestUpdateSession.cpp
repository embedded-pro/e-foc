#include "tools/can_commander/support/UpdateSession.hpp"
#include "gtest/gtest.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <functional>
#include <memory>

namespace
{
    using namespace tool;

    class TestUpdateSession
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

        TestUpdateSession()
        {
            const auto path = QDir{ source.path() }.filePath("published.bin");
            QFile file{ path };
            EXPECT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(payload);

            release.version = ReleaseVersion{ 9, 9, 9 };
            release.tag = "v9.9.9";
            release.pageUrl = QUrl{ "https://github.com/embedded-pro/e-foc/releases/tag/v9.9.9" };
            release.installer = InstallerAsset{ "e_foc-can_commander-v9.9.9-linux-x86_64.deb", QUrl::fromLocalFile(path), payload.size(), QCryptographicHash::hash(payload, QCryptographicHash::Sha256) };
        }

        std::unique_ptr<UpdateSession> Session(std::optional<InstallCommand> command)
        {
            auto session = std::make_unique<UpdateSession>(downloads.path(), [this, command](const QString& installerPath)
                {
                    requestedPaths.append(installerPath);
                    return command;
                });
            QObject::connect(session.get(), &UpdateSession::Relaunch, [this]()
                {
                    ++relaunches;
                });
            QObject::connect(session.get(), &UpdateSession::RestartRequested, [this]()
                {
                    ++restarts;
                });
            return session;
        }

        static InstallCommand Shell(const QString& script, bool exitsBeforeInstalling = false)
        {
            return InstallCommand{ "sh", { "-c", script }, exitsBeforeInstalling };
        }

        static bool WaitFor(const std::function<bool()>& condition, int timeoutMs = 5000)
        {
            QDeadlineTimer deadline{ timeoutMs };
            while (!condition() && !deadline.hasExpired())
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            return condition();
        }

        QString Downloaded() const
        {
            return QDir{ downloads.path() }.filePath("e_foc-can_commander-v9.9.9-linux-x86_64.deb");
        }

        static inline std::unique_ptr<QCoreApplication> application;
        QTemporaryDir source;
        QTemporaryDir downloads;
        const QByteArray payload{ QByteArray{ "package " }.repeated(1024) };
        ReleaseInfo release;
        QStringList requestedPaths;
        int relaunches{ 0 };
        int restarts{ 0 };
    };
}

TEST_F(TestUpdateSession, offers_automatic_install_when_checksum_and_command_exist)
{
    auto session = Session(Shell("exit 0"));
    session->Offer(release);

    EXPECT_EQ(UpdatePhase::available, session->State().phase);
    EXPECT_TRUE(session->State().automaticInstall);
    ASSERT_TRUE(session->State().release.has_value());
    EXPECT_EQ(release.version, session->State().release->version);
}

TEST_F(TestUpdateSession, offers_only_a_download_without_a_checksum)
{
    release.installer->sha256.reset();
    auto session = Session(Shell("exit 0"));
    session->Offer(release);
    session->Install();

    EXPECT_FALSE(session->State().automaticInstall);
    EXPECT_EQ(UpdatePhase::available, session->State().phase);
}

TEST_F(TestUpdateSession, offers_only_a_download_when_the_installation_cannot_update_itself)
{
    auto session = Session(std::nullopt);
    session->Offer(release);

    EXPECT_FALSE(session->State().automaticInstall);
}

TEST_F(TestUpdateSession, installs_the_verified_download_and_asks_for_a_relaunch)
{
    auto session = Session(Shell("exit 0"));
    session->Offer(release);
    session->Install();

    EXPECT_EQ(UpdatePhase::downloading, session->State().phase);
    ASSERT_TRUE(WaitFor([this]()
        {
            return relaunches > 0;
        }));
    EXPECT_EQ(UpdatePhase::installing, session->State().phase);
    EXPECT_EQ(Downloaded(), requestedPaths.last());
    EXPECT_TRUE(QFile::exists(Downloaded()));
    EXPECT_EQ(0, restarts);
}

TEST_F(TestUpdateSession, hands_over_and_asks_for_a_restart_when_the_installer_runs_after_exit)
{
    auto session = Session(Shell("exit 0", true));
    session->Offer(release);
    session->Install();

    ASSERT_TRUE(WaitFor([this]()
        {
            return restarts > 0;
        }));
    EXPECT_EQ(UpdatePhase::installing, session->State().phase);
    EXPECT_EQ(0, relaunches);
}

TEST_F(TestUpdateSession, waits_for_the_bus_to_be_disconnected_before_installing)
{
    auto session = Session(Shell("exit 0"));
    session->SetBusConnected(true);
    session->Offer(release);
    session->Install();

    ASSERT_TRUE(WaitFor([&session]()
        {
            return session->State().phase == UpdatePhase::ready;
        }));
    session->Install();
    EXPECT_EQ(UpdatePhase::ready, session->State().phase);
    EXPECT_TRUE(session->State().busConnected);

    session->SetBusConnected(false);
    session->Install();
    ASSERT_TRUE(WaitFor([this]()
        {
            return relaunches > 0;
        }));
}

TEST_F(TestUpdateSession, reports_a_dismissed_password_prompt)
{
    auto session = Session(Shell("exit 126"));
    session->Offer(release);
    session->Install();

    ASSERT_TRUE(WaitFor([&session]()
        {
            return session->State().phase == UpdatePhase::failed;
        }));
    EXPECT_EQ("the password prompt was dismissed", session->State().failure);
    EXPECT_EQ(0, relaunches);
}

TEST_F(TestUpdateSession, reports_installer_output_on_failure)
{
    auto session = Session(Shell("echo 'dpkg: dependency problems' >&2; exit 100"));
    session->Offer(release);
    session->Install();

    ASSERT_TRUE(WaitFor([&session]()
        {
            return session->State().phase == UpdatePhase::failed;
        }));
    EXPECT_EQ("the installation failed (exit code 100): dpkg: dependency problems", session->State().failure);
}

TEST_F(TestUpdateSession, never_installs_a_download_that_fails_verification)
{
    release.installer->sha256 = QCryptographicHash::hash("tampered", QCryptographicHash::Sha256);
    auto session = Session(Shell("exit 0"));
    session->Offer(release);
    session->Install();

    ASSERT_TRUE(WaitFor([&session]()
        {
            return session->State().phase == UpdatePhase::failed;
        }));
    EXPECT_EQ("the download does not match the checksum published with the release", session->State().failure);
    EXPECT_FALSE(QFile::exists(Downloaded()));

    session->Install();
    EXPECT_EQ(UpdatePhase::downloading, session->State().phase);
    ASSERT_TRUE(WaitFor([&session]()
        {
            return session->State().phase == UpdatePhase::failed;
        }));
    EXPECT_EQ(0, relaunches);
}

TEST_F(TestUpdateSession, cancel_returns_to_the_offer)
{
    auto session = Session(Shell("exit 0"));
    session->Offer(release);
    session->Install();
    session->Cancel();

    EXPECT_EQ(UpdatePhase::available, session->State().phase);
    EXPECT_FALSE(WaitFor([this]()
        {
            return relaunches > 0;
        },
        300));
    EXPECT_FALSE(QFile::exists(Downloaded()));
}

TEST_F(TestUpdateSession, keeps_the_download_in_progress_when_a_newer_release_is_found)
{
    auto session = Session(Shell("exit 0"));
    session->Offer(release);
    session->SetBusConnected(true);
    session->Install();

    auto newer = release;
    newer.version = ReleaseVersion{ 10, 0, 0 };
    session->Offer(newer);

    ASSERT_TRUE(session->State().release.has_value());
    EXPECT_EQ(release.version, session->State().release->version);
}

TEST_F(TestUpdateSession, dismiss_hides_the_offer)
{
    auto session = Session(Shell("exit 0"));
    session->Offer(release);
    session->Dismiss();

    EXPECT_EQ(UpdatePhase::idle, session->State().phase);
}
