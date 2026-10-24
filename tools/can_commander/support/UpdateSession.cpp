#include "tools/can_commander/support/UpdateSession.hpp"
#include <QDir>

namespace tool
{
    UpdateSession::UpdateSession(const QString& downloadDirectory, CommandFactory commandFor, QObject* parent)
        : QObject(parent)
        , downloadDirectory(downloadDirectory)
        , commandFor(std::move(commandFor))
    {
        connect(&downloader, &UpdateDownloader::Progress, this, [this](qint64 received, qint64 total)
            {
                state.received = received;
                state.total = total;
                emit StateChanged(state);
            });
        connect(&downloader, &UpdateDownloader::Downloaded, this, [this](const QString& path)
            {
                downloadedPath = path;
                if (state.busConnected)
                    Enter(UpdatePhase::ready);
                else
                    StartInstall();
            });
        connect(&downloader, &UpdateDownloader::Failed, this, &UpdateSession::Fail);
        connect(&installer, &UpdateInstaller::HandedOver, this, &UpdateSession::RestartRequested);
        connect(&installer, &UpdateInstaller::Installed, this, &UpdateSession::Relaunch);
        connect(&installer, &UpdateInstaller::Failed, this, &UpdateSession::Fail);
    }

    void UpdateSession::Offer(const ReleaseInfo& release)
    {
        if (Busy())
            return;

        state.release = release;
        state.automaticInstall = release.installer && release.installer->sha256 && commandFor(InstallerPath(*release.installer)).has_value();
        state.failure.clear();
        Enter(UpdatePhase::available);
    }

    void UpdateSession::Install()
    {
        switch (state.phase)
        {
            case UpdatePhase::available:
            case UpdatePhase::failed:
                if (state.automaticInstall)
                    StartDownload();
                break;
            case UpdatePhase::ready:
                if (!state.busConnected)
                    StartInstall();
                break;
            default:
                break;
        }
    }

    void UpdateSession::Cancel()
    {
        if (state.phase == UpdatePhase::downloading)
            downloader.Cancel();

        if (state.phase == UpdatePhase::downloading || state.phase == UpdatePhase::ready)
            Enter(UpdatePhase::available);
    }

    void UpdateSession::Dismiss()
    {
        if (state.phase == UpdatePhase::installing)
            return;

        if (state.phase == UpdatePhase::downloading)
            downloader.Cancel();

        Enter(UpdatePhase::idle);
    }

    void UpdateSession::SetBusConnected(bool connected)
    {
        if (state.busConnected == connected)
            return;

        state.busConnected = connected;
        emit StateChanged(state);
    }

    const UpdateState& UpdateSession::State() const
    {
        return state;
    }

    bool UpdateSession::Busy() const
    {
        return state.phase == UpdatePhase::downloading || state.phase == UpdatePhase::ready || state.phase == UpdatePhase::installing;
    }

    QString UpdateSession::InstallerPath(const InstallerAsset& asset) const
    {
        return QDir{ downloadDirectory }.filePath(asset.fileName);
    }

    void UpdateSession::StartDownload()
    {
        state.received = 0;
        state.total = state.release->installer->size;
        state.failure.clear();
        Enter(UpdatePhase::downloading);
        downloader.Start(*state.release->installer, downloadDirectory);
    }

    void UpdateSession::StartInstall()
    {
        const auto command = commandFor(downloadedPath);
        if (!command)
        {
            Fail("this installation cannot be updated automatically");
            return;
        }

        Enter(UpdatePhase::installing);
        installer.Install(*command);
    }

    void UpdateSession::Fail(const QString& reason)
    {
        state.failure = reason;
        Enter(UpdatePhase::failed);
    }

    void UpdateSession::Enter(UpdatePhase phase)
    {
        state.phase = phase;
        emit StateChanged(state);
    }
}
