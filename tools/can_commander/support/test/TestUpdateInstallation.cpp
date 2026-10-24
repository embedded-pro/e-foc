#include "tools/can_commander/support/UpdateInstallation.hpp"
#include "gtest/gtest.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtGlobal>

namespace
{
    using namespace tool;

    class TestUpdateInstallation
        : public testing::Test
    {
    public:
        static QString DecodedScript(const InstallCommand& command)
        {
            const auto index = command.arguments.indexOf("-EncodedCommand");
            if (index < 0 || index + 1 >= command.arguments.size())
                return {};

            const auto bytes = QByteArray::fromBase64(command.arguments.at(index + 1).toLatin1());
            return QString::fromUtf16(reinterpret_cast<const char16_t*>(bytes.constData()), bytes.size() / 2);
        }

        QString WindowsLayout(bool withUninstaller)
        {
            EXPECT_TRUE(QDir{ installation.path() }.mkpath("bin"));
            if (withUninstaller)
            {
                QFile uninstaller{ QDir{ installation.path() }.filePath("Uninstall.exe") };
                EXPECT_TRUE(uninstaller.open(QIODevice::WriteOnly));
            }
            return QDir{ installation.path() }.filePath("bin/e-foc-can-commander.exe");
        }

        QString FakeExecutable(const QString& name)
        {
            const auto path = QDir{ tools.path() }.filePath(name);
            QFile file{ path };
            EXPECT_TRUE(file.open(QIODevice::WriteOnly));
            file.write("#!/bin/sh\n");
            file.close();
            EXPECT_TRUE(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
            return path;
        }

        void UseOnlyFakeTools()
        {
            qputenv("PATH", QFile::encodeName(tools.path()));
        }

        void TearDown() override
        {
            qputenv("PATH", originalPath);
        }

        QTemporaryDir installation;
        QTemporaryDir tools;
        const QByteArray originalPath{ qgetenv("PATH") };
    };
}

TEST_F(TestUpdateInstallation, windows_installation_with_an_uninstaller_is_a_package)
{
    EXPECT_EQ(InstallationKind::package, DetectInstallation(InstallerPlatform::windowsX64, WindowsLayout(true)));
}

TEST_F(TestUpdateInstallation, windows_installation_without_an_uninstaller_is_portable)
{
    EXPECT_EQ(InstallationKind::portable, DetectInstallation(InstallerPlatform::windowsX64, WindowsLayout(false)));
}

TEST_F(TestUpdateInstallation, linux_installation_under_usr_is_a_package)
{
    EXPECT_EQ(InstallationKind::package, DetectInstallation(InstallerPlatform::linuxX64, "/usr/bin/e-foc-can-commander"));
    EXPECT_EQ(InstallationKind::portable, DetectInstallation(InstallerPlatform::linuxX64, "/home/user/tools/e-foc-can-commander"));
    EXPECT_EQ(InstallationKind::portable, DetectInstallation(InstallerPlatform::linuxX64, "/usr/../home/user/e-foc-can-commander"));
}

TEST_F(TestUpdateInstallation, unsupported_platform_is_portable)
{
    EXPECT_EQ(InstallationKind::portable, DetectInstallation(InstallerPlatform::unsupported, "/usr/bin/e-foc-can-commander"));
}

TEST_F(TestUpdateInstallation, windows_install_directory_is_the_parent_of_bin)
{
    EXPECT_EQ(QDir::cleanPath(installation.path()), WindowsInstallDirectory(WindowsLayout(true)));
}

TEST_F(TestUpdateInstallation, windows_command_starts_the_installer_elevated_after_the_tool_exits)
{
    const auto command = WindowsUpdateCommand("C:\\Temp\\setup.exe", "C:\\Program Files\\e-foc CAN Commander", 4242, "C:\\Program Files\\e-foc CAN Commander\\bin\\e-foc-can-commander.exe");
    const auto script = DecodedScript(command);

    EXPECT_EQ("powershell.exe", command.program);
    EXPECT_TRUE(command.exitsBeforeInstalling);
    EXPECT_TRUE(command.arguments.contains("-NoProfile"));
    EXPECT_TRUE(script.startsWith("Wait-Process -Id 4242 ")) << script.toStdString();
    EXPECT_TRUE(script.contains("Start-Process -FilePath 'C:\\Temp\\setup.exe' -ArgumentList '/S /RELAUNCH /D=C:\\Program Files\\e-foc CAN Commander' -Verb RunAs")) << script.toStdString();
}

TEST_F(TestUpdateInstallation, windows_command_restarts_the_old_version_when_elevation_is_declined)
{
    const auto script = DecodedScript(WindowsUpdateCommand("C:\\Temp\\setup.exe", "C:\\Apps\\CAN", 1, "C:\\Apps\\CAN\\bin\\e-foc-can-commander.exe"));

    EXPECT_TRUE(script.contains("catch { Start-Process -FilePath 'C:\\Apps\\CAN\\bin\\e-foc-can-commander.exe' }")) << script.toStdString();
}

TEST_F(TestUpdateInstallation, windows_command_escapes_quotes_in_paths)
{
    const auto script = DecodedScript(WindowsUpdateCommand("C:\\Users\\O'Brien\\setup.exe", "C:\\Apps\\CAN", 1, "C:\\Apps\\CAN\\bin\\e-foc-can-commander.exe"));

    EXPECT_TRUE(script.contains("'C:\\Users\\O''Brien\\setup.exe'")) << script.toStdString();
}

TEST_F(TestUpdateInstallation, linux_command_installs_the_package_through_elevation)
{
    const auto command = LinuxUpdateCommand("/usr/bin/pkexec", "/usr/bin/apt-get", "/tmp/updates/e_foc-can_commander-v9.9.9-linux-x86_64.deb");

    EXPECT_EQ("/usr/bin/pkexec", command.program);
    EXPECT_EQ((QStringList{ "/usr/bin/apt-get", "install", "-y", "/tmp/updates/e_foc-can_commander-v9.9.9-linux-x86_64.deb" }), command.arguments);
    EXPECT_FALSE(command.exitsBeforeInstalling);
}

TEST_F(TestUpdateInstallation, portable_installation_has_no_update_command)
{
    EXPECT_FALSE(DefaultUpdateCommand(InstallerPlatform::linuxX64, "/tmp/x.deb", "/home/user/e-foc-can-commander", 1).has_value());
    EXPECT_FALSE(DefaultUpdateCommand(InstallerPlatform::windowsX64, "C:\\Temp\\setup.exe", WindowsLayout(false), 1).has_value());
}

TEST_F(TestUpdateInstallation, windows_package_updates_its_own_installation_folder)
{
    const auto application = WindowsLayout(true);
    const auto command = DefaultUpdateCommand(InstallerPlatform::windowsX64, "C:\\Temp\\setup.exe", application, 7);

    ASSERT_TRUE(command.has_value());
    EXPECT_TRUE(DecodedScript(*command).contains("/D=" + QDir::toNativeSeparators(QDir::cleanPath(installation.path())) + "'"));
}

TEST_F(TestUpdateInstallation, linux_package_installs_through_pkexec_and_apt_get)
{
    const auto pkexec = FakeExecutable("pkexec");
    const auto aptGet = FakeExecutable("apt-get");
    UseOnlyFakeTools();

    const auto command = DefaultUpdateCommand(InstallerPlatform::linuxX64, "/tmp/e_foc-can_commander-v9.9.9-linux-x86_64.deb", "/usr/bin/e-foc-can-commander", 1);

    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(pkexec, command->program);
    EXPECT_EQ((QStringList{ aptGet, "install", "-y", "/tmp/e_foc-can_commander-v9.9.9-linux-x86_64.deb" }), command->arguments);
    EXPECT_FALSE(command->exitsBeforeInstalling);
}

TEST_F(TestUpdateInstallation, linux_package_without_pkexec_has_no_update_command)
{
    FakeExecutable("apt-get");
    UseOnlyFakeTools();

    EXPECT_FALSE(DefaultUpdateCommand(InstallerPlatform::linuxX64, "/tmp/x.deb", "/usr/bin/e-foc-can-commander", 1).has_value());
}

TEST_F(TestUpdateInstallation, linux_package_without_apt_get_has_no_update_command)
{
    FakeExecutable("pkexec");
    UseOnlyFakeTools();

    EXPECT_FALSE(DefaultUpdateCommand(InstallerPlatform::linuxX64, "/tmp/x.deb", "/usr/bin/e-foc-can-commander", 1).has_value());
}
