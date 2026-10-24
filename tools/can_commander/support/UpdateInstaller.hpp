#pragma once

#include "tools/can_commander/support/UpdateInstallation.hpp"
#include <QObject>
#include <QProcess>

namespace tool
{
    class UpdateInstaller
        : public QObject
    {
        Q_OBJECT

    public:
        explicit UpdateInstaller(QObject* parent = nullptr);

        void Install(const InstallCommand& command);
        bool IsInstalling() const;

    signals:
        void HandedOver();
        void Installed();
        void Failed(const QString& reason);

    private:
        void OnFinished(int exitCode, QProcess::ExitStatus status);

        QProcess process;
    };
}
