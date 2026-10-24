#include "tools/can_commander/support/UpdateInstaller.hpp"

namespace tool
{
    namespace
    {
        constexpr int authenticationDismissed{ 126 };
        constexpr int notAuthorized{ 127 };
    }

    UpdateInstaller::UpdateInstaller(QObject* parent)
        : QObject(parent)
    {
        process.setProcessChannelMode(QProcess::MergedChannels);
        connect(&process, &QProcess::finished, this, &UpdateInstaller::OnFinished);
        connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error)
            {
                if (error == QProcess::FailedToStart)
                    emit Failed(QString("could not start %1: %2").arg(process.program(), process.errorString()));
            });
    }

    void UpdateInstaller::Install(const InstallCommand& command)
    {
        if (IsInstalling())
            return;

        if (command.exitsBeforeInstalling)
        {
            if (QProcess::startDetached(command.program, command.arguments))
                emit HandedOver();
            else
                emit Failed(QString("could not start %1").arg(command.program));
            return;
        }

        process.start(command.program, command.arguments);
    }

    bool UpdateInstaller::IsInstalling() const
    {
        return process.state() != QProcess::NotRunning;
    }

    void UpdateInstaller::OnFinished(int exitCode, QProcess::ExitStatus status)
    {
        if (status == QProcess::NormalExit && exitCode == 0)
        {
            emit Installed();
            return;
        }

        if (status == QProcess::CrashExit)
            emit Failed(QString("%1 stopped unexpectedly").arg(process.program()));
        else if (exitCode == authenticationDismissed)
            emit Failed("the password prompt was dismissed");
        else if (exitCode == notAuthorized)
            emit Failed("this account may not install software");
        else
        {
            const auto output = QString::fromLocal8Bit(process.readAll()).trimmed().split('\n').last();
            emit Failed(QString("the installation failed (exit code %1)%2").arg(exitCode).arg(output.isEmpty() ? QString{} : ": " + output));
        }
    }
}
