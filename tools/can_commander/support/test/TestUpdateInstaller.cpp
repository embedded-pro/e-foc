#include "tools/can_commander/support/UpdateInstaller.hpp"
#include "gtest/gtest.h"
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <functional>
#include <memory>
#include <optional>

namespace
{
    using namespace tool;

    class TestUpdateInstaller
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

        TestUpdateInstaller()
        {
            QObject::connect(&installer, &UpdateInstaller::Installed, [this]()
                {
                    ++installed;
                });
            QObject::connect(&installer, &UpdateInstaller::HandedOver, [this]()
                {
                    ++handedOver;
                });
            QObject::connect(&installer, &UpdateInstaller::Failed, [this](const QString& reason)
                {
                    failure = reason;
                });
        }

        static InstallCommand Shell(const QString& script, bool exitsBeforeInstalling = false)
        {
            return InstallCommand{ "sh", { "-c", script }, exitsBeforeInstalling };
        }

        bool Finished() const
        {
            return installed > 0 || failure.has_value();
        }

        bool WaitUntilFinished(int timeoutMs = 5000)
        {
            QDeadlineTimer deadline{ timeoutMs };
            while (!Finished() && !deadline.hasExpired())
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QCoreApplication::processEvents();
            return Finished();
        }

        static inline std::unique_ptr<QCoreApplication> application;
        UpdateInstaller installer;
        int installed{ 0 };
        int handedOver{ 0 };
        std::optional<QString> failure;
    };
}

TEST_F(TestUpdateInstaller, reports_a_successful_installation)
{
    installer.Install(Shell("exit 0"));

    ASSERT_TRUE(WaitUntilFinished());
    EXPECT_EQ(1, installed);
    EXPECT_FALSE(failure.has_value());
    EXPECT_FALSE(installer.IsInstalling());
}

TEST_F(TestUpdateInstaller, reports_a_dismissed_password_prompt)
{
    installer.Install(Shell("exit 126"));

    ASSERT_TRUE(WaitUntilFinished());
    EXPECT_EQ("the password prompt was dismissed", failure.value_or(""));
    EXPECT_EQ(0, installed);
}

TEST_F(TestUpdateInstaller, reports_an_account_that_may_not_install)
{
    installer.Install(Shell("exit 127"));

    ASSERT_TRUE(WaitUntilFinished());
    EXPECT_EQ("this account may not install software", failure.value_or(""));
}

TEST_F(TestUpdateInstaller, reports_the_last_line_of_output_when_installation_fails)
{
    installer.Install(Shell("echo 'Reading package lists'; echo 'E: broken package'; exit 3"));

    ASSERT_TRUE(WaitUntilFinished());
    EXPECT_EQ("the installation failed (exit code 3): E: broken package", failure.value_or(""));
}

TEST_F(TestUpdateInstaller, reports_the_exit_code_alone_when_the_installer_is_silent)
{
    installer.Install(Shell("exit 3"));

    ASSERT_TRUE(WaitUntilFinished());
    EXPECT_EQ("the installation failed (exit code 3)", failure.value_or(""));
}

TEST_F(TestUpdateInstaller, reports_an_installer_that_crashes)
{
    installer.Install(Shell("kill -SEGV $$"));

    ASSERT_TRUE(WaitUntilFinished());
    EXPECT_EQ("sh stopped unexpectedly", failure.value_or(""));
}

TEST_F(TestUpdateInstaller, reports_an_installer_that_cannot_start)
{
    installer.Install(InstallCommand{ "/nonexistent/e-foc-installer", {}, false });

    ASSERT_TRUE(WaitUntilFinished());
    ASSERT_TRUE(failure.has_value());
    EXPECT_TRUE(failure->startsWith("could not start /nonexistent/e-foc-installer")) << failure->toStdString();
}

TEST_F(TestUpdateInstaller, hands_over_to_an_installer_that_runs_after_exit)
{
    installer.Install(Shell("exit 0", true));

    EXPECT_EQ(1, handedOver);
    EXPECT_FALSE(failure.has_value());
    EXPECT_FALSE(installer.IsInstalling());
}

TEST_F(TestUpdateInstaller, reports_a_detached_installer_that_cannot_start)
{
    installer.Install(InstallCommand{ "/nonexistent/e-foc-installer", {}, true });

    EXPECT_EQ(0, handedOver);
    EXPECT_EQ("could not start /nonexistent/e-foc-installer", failure.value_or(""));
}

TEST_F(TestUpdateInstaller, ignores_a_second_installation_while_one_is_running)
{
    installer.Install(Shell("sleep 0.3; exit 0"));
    ASSERT_TRUE(installer.IsInstalling());

    installer.Install(Shell("exit 126"));

    ASSERT_TRUE(WaitUntilFinished());
    EXPECT_EQ(1, installed);
    EXPECT_FALSE(failure.has_value());
}
