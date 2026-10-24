#include "tools/can_commander/support/UpdateInstallation.hpp"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace tool
{
    namespace
    {
        QString PowerShellLiteral(const QString& text)
        {
            return "'" + QString{ text }.replace("'", "''") + "'";
        }

        QString EncodedPowerShellCommand(const QString& script)
        {
            const auto* utf16 = reinterpret_cast<const char*>(script.utf16());
            return QString::fromLatin1(QByteArray{ utf16, script.size() * static_cast<qsizetype>(sizeof(char16_t)) }.toBase64());
        }
    }

    InstallationKind DetectInstallation(InstallerPlatform platform, const QString& applicationFilePath)
    {
        switch (platform)
        {
            case InstallerPlatform::windowsX64:
                return QFileInfo::exists(QDir{ WindowsInstallDirectory(applicationFilePath) }.filePath("Uninstall.exe")) ? InstallationKind::package : InstallationKind::portable;
            case InstallerPlatform::linuxX64:
                return QDir::cleanPath(applicationFilePath).startsWith("/usr/") ? InstallationKind::package : InstallationKind::portable;
            default:
                return InstallationKind::portable;
        }
    }

    QString WindowsInstallDirectory(const QString& applicationFilePath)
    {
        return QDir::cleanPath(QFileInfo{ applicationFilePath }.absolutePath() + "/..");
    }

    InstallCommand WindowsUpdateCommand(const QString& installerPath, const QString& installDirectory, qint64 processId, const QString& applicationFilePath)
    {
        const auto arguments = "/S /RELAUNCH /D=" + QDir::toNativeSeparators(installDirectory);
        const auto script = QString("Wait-Process -Id %1 -ErrorAction SilentlyContinue\n"
                                    "try { Start-Process -FilePath %2 -ArgumentList %3 -Verb RunAs -ErrorAction Stop }\n"
                                    "catch { Start-Process -FilePath %4 }\n")
                                .arg(processId)
                                .arg(PowerShellLiteral(QDir::toNativeSeparators(installerPath)), PowerShellLiteral(arguments), PowerShellLiteral(QDir::toNativeSeparators(applicationFilePath)));

        return InstallCommand{ "powershell.exe", { "-NoProfile", "-NonInteractive", "-WindowStyle", "Hidden", "-EncodedCommand", EncodedPowerShellCommand(script) }, true };
    }

    InstallCommand LinuxUpdateCommand(const QString& elevationProgram, const QString& packageManager, const QString& packagePath)
    {
        return InstallCommand{ elevationProgram, { packageManager, "install", "-y", QFileInfo{ packagePath }.absoluteFilePath() }, false };
    }

    std::optional<InstallCommand> DefaultUpdateCommand(InstallerPlatform platform, const QString& installerPath, const QString& applicationFilePath, qint64 processId)
    {
        if (DetectInstallation(platform, applicationFilePath) != InstallationKind::package)
            return std::nullopt;

        if (platform == InstallerPlatform::windowsX64)
            return WindowsUpdateCommand(installerPath, WindowsInstallDirectory(applicationFilePath), processId, applicationFilePath);

        if (platform == InstallerPlatform::linuxX64)
        {
            const auto elevation = QStandardPaths::findExecutable("pkexec");
            const auto packageManager = QStandardPaths::findExecutable("apt-get");
            if (elevation.isEmpty() || packageManager.isEmpty())
                return std::nullopt;

            return LinuxUpdateCommand(elevation, packageManager, installerPath);
        }

        return std::nullopt;
    }
}
