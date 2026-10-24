#pragma once

#include "tools/can_commander/support/ReleaseFeed.hpp"
#include "tools/can_commander/support/UpdateDownloader.hpp"
#include "tools/can_commander/support/UpdateInstaller.hpp"
#include <QObject>
#include <functional>
#include <optional>

namespace tool
{
    enum class UpdatePhase
    {
        idle,
        available,
        downloading,
        ready,
        installing,
        failed
    };

    struct UpdateState
    {
        UpdatePhase phase{ UpdatePhase::idle };
        std::optional<ReleaseInfo> release;
        bool automaticInstall{ false };
        bool busConnected{ false };
        qint64 received{ 0 };
        qint64 total{ 0 };
        QString failure;
    };

    class UpdateSession
        : public QObject
    {
        Q_OBJECT

    public:
        using CommandFactory = std::function<std::optional<InstallCommand>(const QString& installerPath)>;

        UpdateSession(const QString& downloadDirectory, CommandFactory commandFor, QObject* parent = nullptr);

        void Offer(const ReleaseInfo& release);
        void Install();
        void Cancel();
        void Dismiss();
        void SetBusConnected(bool connected);

        const UpdateState& State() const;

    signals:
        void StateChanged(const tool::UpdateState& state);
        void RestartRequested();
        void Relaunch();

    private:
        bool Busy() const;
        QString InstallerPath() const;
        void StartDownload();
        void StartInstall();
        void Fail(const QString& reason);
        void Enter(UpdatePhase phase);

        QString downloadDirectory;
        CommandFactory commandFor;
        UpdateDownloader downloader;
        UpdateInstaller installer;
        UpdateState state;
        QString downloadedPath;
    };
}
