#include "tools/can_commander/gui/MotorFirmware.hpp"
#include "tools/can_commander/logic/FirmwareRefusal.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <chrono>

namespace tool
{
    namespace
    {
        using ui::backend::qt::ToQt;
        using ui::theme::StatusLevel;

        constexpr qint64 bytesPerSecond{ 2500 };

        QString Version(const services::FirmwareVersion& version)
        {
            return QString::fromStdString(VersionText(version));
        }

        QString Kilobytes(qint64 bytes)
        {
            return QString::number(static_cast<double>(bytes) / 1024.0, 'f', 1) + " KB";
        }

        QString Duration(std::chrono::seconds seconds)
        {
            if (seconds.count() < 60)
                return QString("%1 s").arg(seconds.count());
            return QString("%1 min %2 s").arg(seconds.count() / 60).arg(seconds.count() % 60);
        }

        bool IsStopped(std::optional<FocMotorState> state)
        {
            return state == FocMotorState::idle || state == FocMotorState::partialCalibration || state == FocMotorState::ready;
        }

        std::optional<std::vector<uint8_t>> ReadPackFile(const QString& path)
        {
            QFile file{ path };
            if (!file.open(QIODevice::ReadOnly))
                return std::nullopt;

            const auto bytes = file.read(static_cast<qint64>(stagingCapacity) + 1);
            const auto* data = reinterpret_cast<const uint8_t*>(bytes.constData());
            return std::vector<uint8_t>{ data, data + bytes.size() };
        }
    }

    MotorFirmware::MotorFirmware(CanCommandClient& client, FirmwareTransfer& transfer, uint16_t nodeId, FirmwareTab& tab, Reporter reporter, Reason commandsUnavailable, QWidget* dialogParent)
        : QObject(dialogParent)
        , client(client)
        , transfer(transfer)
        , nodeId(nodeId)
        , tab(tab)
        , reporter(std::move(reporter))
        , commandsUnavailable(std::move(commandsUnavailable))
        , dialogParent(dialogParent)
    {
        ConnectTab();
        ConnectDownloader();
    }

    void MotorFirmware::ConnectTab()
    {
        connect(&tab, &FirmwareTab::OpenPackRequested, this, &MotorFirmware::OpenPack);
        connect(&tab, &FirmwareTab::DownloadRequested, this, &MotorFirmware::Download);
        connect(&tab, &FirmwareTab::TryAgainRequested, this, &MotorFirmware::Download);
        connect(&tab, &FirmwareTab::ApplyRequested, this, &MotorFirmware::Apply);
        connect(&tab, &FirmwareTab::DiscardRequested, this, &MotorFirmware::Discard);
        connect(&tab, &FirmwareTab::CancelRequested, this, [this]()
            {
                if (fetching)
                    downloader.Cancel();
                else
                    transfer.Cancel();
            });
    }

    void MotorFirmware::ConnectDownloader()
    {
        connect(&downloader, &UpdateDownloader::Progress, this, [this](qint64 received, qint64 total)
            {
                tab.ShowProgress(received, total, QString("Downloading the release pack: %1 of %2").arg(Kilobytes(received), Kilobytes(total)));
            });
        connect(&downloader, &UpdateDownloader::Downloaded, this, &MotorFirmware::OnReleasePackFetched);
        connect(&downloader, &UpdateDownloader::Failed, this, [this](const QString& reason)
            {
                fetching = false;
                tab.HideProgress();
                Fail(QString("The release pack was not downloaded: %1").arg(reason));
            });
    }

    bool MotorFirmware::UpdateAvailable() const
    {
        if (!identity || !latestRelease || !ReleasePack())
            return false;

        const ReleaseVersion running{ identity->running.major, identity->running.minor, identity->running.patch };
        return latestRelease->version > running;
    }

    QString MotorFirmware::UpdateSummary() const
    {
        return UpdateAvailable() ? QString("firmware %1 available").arg(ToQt(latestRelease->version.ToString())) : QString();
    }

    bool MotorFirmware::IsDownloading() const
    {
        const auto phase = transfer.Phase();
        return transfer.ActiveNode() == nodeId && (phase == TransferPhase::erasing || phase == TransferPhase::sending || phase == TransferPhase::verifying || phase == TransferPhase::cancelling);
    }

    void MotorFirmware::SetLatestRelease(const ReleaseInfo& release)
    {
        latestRelease = release;
        Refresh();
    }

    void MotorFirmware::SetMotorState(std::optional<FocMotorState> state)
    {
        motorState = state;
        Refresh();
    }

    void MotorFirmware::OnFirmwareIdentity(const FirmwareIdentity& reported)
    {
        identity = reported;
        Refresh();
    }

    void MotorFirmware::ForgetNodeState()
    {
        identity.reset();
        motorState.reset();
        Refresh();
    }

    void MotorFirmware::OpenPack()
    {
        const auto path = QFileDialog::getOpenFileName(dialogParent, "Open Pack File", {}, "Upgrade packs (*.upgrade);;All files (*)");
        if (path.isEmpty())
            return;

        const auto bytes = ReadPackFile(path);
        if (!bytes)
        {
            Fail(QString("Cannot read %1.").arg(QDir::toNativeSeparators(path)));
            return;
        }

        if (Offer(*bytes, QFileInfo(path).fileName(), false))
            ShowOutcome(QString("%1 holds firmware %2 for this board; Download to Controller sends it.").arg(chosenSource, Version(chosenPack->version)), StatusLevel::Neutral);
    }

    void MotorFirmware::Download()
    {
        if (chosenPack)
        {
            if (ConfirmDownload())
                StartTransfer();
            return;
        }

        if (const auto pack = ReleasePack())
            FetchReleasePack(*pack);
    }

    void MotorFirmware::FetchReleasePack(const InstallerAsset& asset)
    {
        fetching = true;
        downloadFailed = false;
        Report(IssueSeverity::info, QString("downloading %1").arg(asset.fileName));
        tab.ShowProgress(0, asset.size, "Downloading the release pack…");
        downloader.Start(asset, QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/firmware");
        Refresh();
    }

    void MotorFirmware::OnReleasePackFetched(const QString& path)
    {
        fetching = false;
        tab.HideProgress();
        const auto bytes = ReadPackFile(path);
        if (!bytes)
        {
            Fail(QString("Cannot read the downloaded pack %1.").arg(QDir::toNativeSeparators(path)));
            return;
        }

        if (Offer(*bytes, QString("release %1").arg(latestRelease ? latestRelease->tag : QString()), true) && ConfirmDownload())
            StartTransfer();
        Refresh();
    }

    bool MotorFirmware::Offer(const std::vector<uint8_t>& bytes, const QString& source, bool fromRelease)
    {
        const auto board = Board();
        if (!board)
            return false;

        const auto inspection = InspectPack(bytes, *board);
        if (!inspection.pack)
        {
            chosenPack.reset();
            Fail(QString("%1 is not offered: %2").arg(source, QString::fromStdString(DescribePackProblem(inspection, *board))));
            return false;
        }

        chosenPack = inspection.pack;
        chosenSource = source;
        downloadFailed = false;
        ClearOutcome();
        Report(IssueSeverity::info, QString("%1 %2 checked: firmware %3 for the %4").arg(fromRelease ? "release pack" : "pack file", source, Version(chosenPack->version), ToQt(board->name)));
        Refresh();
        return true;
    }

    bool MotorFirmware::ConfirmDownload()
    {
        const auto size = static_cast<qint64>(chosenPack->bytes.size());
        const auto text = QString("Download firmware %1 to %2?\n\n%2 stays stopped until the download ends, about %3 for %4. Nothing is installed yet: the pack waits on the controller until you apply it.")
                              .arg(Version(chosenPack->version), MotorName(), Duration(std::chrono::seconds{ size / bytesPerSecond + 1 }), Kilobytes(size));
        return QMessageBox::question(dialogParent, "Download to Controller", text, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes;
    }

    void MotorFirmware::StartTransfer()
    {
        if (!chosenPack || !DownloadReason().isEmpty())
            return;

        downloadFailed = false;
        ClearOutcome();
        transferClock.start();
        if (transfer.StartDownload(nodeId, *chosenPack))
            Report(IssueSeverity::info, QString("download of firmware %1 (%2) started").arg(Version(chosenPack->version), chosenSource));
        emit TransferChanged();
    }

    void MotorFirmware::Apply()
    {
        if (!HasStagedPack())
            return;

        const auto text = QString("Apply firmware %1 to %2?\n\n%2 runs %3. The controller restarts and its bootloader installs %1; the old firmware is not kept.")
                              .arg(Version(identity->staged), MotorName(), Version(identity->running));
        if (QMessageBox::question(dialogParent, "Apply Firmware", text, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
            return;

        ClearOutcome();
        if (transfer.Apply(nodeId, identity->staged))
            Report(IssueSeverity::info, QString("apply of firmware %1 over %2 started").arg(Version(identity->staged), Version(identity->running)));
        emit TransferChanged();
    }

    void MotorFirmware::Discard()
    {
        if (HasStagedPack() && transfer.Discard(nodeId))
            Report(IssueSeverity::info, QString("discarding staged firmware %1").arg(Version(identity->staged)));
        emit TransferChanged();
    }

    void MotorFirmware::OnTransferProgress(const TransferProgress& progress)
    {
        switch (progress.phase)
        {
            case TransferPhase::erasing:
                tab.ShowBusy("Erasing…");
                break;
            case TransferPhase::sending:
            {
                const auto left = TimeLeft(progress.bytesSent, progress.totalBytes, std::chrono::milliseconds{ transferClock.elapsed() });
                const auto format = QString("%1 of %2%3").arg(Kilobytes(progress.bytesSent), Kilobytes(progress.totalBytes), left ? QString(", about %1 left").arg(Duration(*left)) : QString());
                tab.ShowProgress(progress.bytesSent, progress.totalBytes, format);
                break;
            }
            case TransferPhase::verifying:
                tab.ShowBusy("Verifying…");
                break;
            case TransferPhase::restarting:
                tab.ShowBusy("Restarting: waiting for the controller to come back…");
                break;
            default:
                break;
        }
        Refresh();
    }

    void MotorFirmware::OnDownloadFinished(DownloadOutcome outcome, services::FwuError error)
    {
        tab.HideProgress();
        const auto version = chosenPack ? Version(chosenPack->version) : QString();
        switch (outcome)
        {
            case DownloadOutcome::staged:
                Succeed(QString("Firmware %1 downloaded, apply when ready.").arg(version));
                client.RequestFirmwareIdentity(nodeId);
                break;
            case DownloadOutcome::cancelled:
                Succeed("Download cancelled; nothing was staged.", IssueSeverity::warning);
                break;
            case DownloadOutcome::refused:
                FailDownload(QString("The controller refused firmware %1: %2").arg(version, QString::fromStdString(FirmwareRefusalText(error, FirmwareStep::download))));
                break;
            case DownloadOutcome::noAnswer:
                FailDownload(QString("The controller stopped answering; the download of firmware %1 was given up.").arg(version));
                break;
            default:
                FailDownload("The CAN adapter refused a frame; the download stopped.");
                break;
        }
        emit TransferChanged();
    }

    void MotorFirmware::FailDownload(const QString& message)
    {
        downloadFailed = true;
        Fail(message);
    }

    void MotorFirmware::OnDiscardFinished(bool acknowledged)
    {
        if (acknowledged)
            Succeed("The staged pack was discarded.");
        else
            Fail("The controller did not acknowledge the discard; the pack may still be staged.");
        client.RequestFirmwareIdentity(nodeId);
        emit TransferChanged();
    }

    void MotorFirmware::OnApplyFinished(ApplyOutcome outcome, services::FwuError error, const services::FirmwareVersion& running)
    {
        tab.HideProgress();
        switch (outcome)
        {
            case ApplyOutcome::updated:
                Succeed(QString("%1 updated to firmware %2.").arg(MotorName(), Version(running)));
                break;
            case ApplyOutcome::oldVersionKept:
                Fail(QString("%1 came back with firmware %2: its bootloader refused the pack and kept the old firmware.").arg(MotorName(), Version(running)));
                break;
            case ApplyOutcome::noNode:
                Fail(QString("%1 did not come back within 30 s: the update failed, and recovering the controller needs a debugger.").arg(MotorName()));
                break;
            case ApplyOutcome::refused:
                Fail(QString("The controller refused to apply the pack: %1").arg(QString::fromStdString(FirmwareRefusalText(error, FirmwareStep::apply))));
                break;
            default:
                Fail("The controller did not answer Activate; nothing was installed.");
                break;
        }
        emit TransferChanged();
    }

    void MotorFirmware::OnTransferAbandoned()
    {
        tab.HideProgress();
        FailDownload("The download was abandoned when the bus closed; the controller drops it after 30 s without a command.");
        emit TransferChanged();
    }

    void MotorFirmware::Fail(const QString& message)
    {
        ShowOutcome(message, StatusLevel::Fault);
        Report(IssueSeverity::error, message);
    }

    void MotorFirmware::Succeed(const QString& message, IssueSeverity severity)
    {
        downloadFailed = false;
        ShowOutcome(message, severity == IssueSeverity::info ? StatusLevel::Ok : StatusLevel::Warning);
        Report(severity, message);
    }

    void MotorFirmware::ShowOutcome(const QString& message, StatusLevel level)
    {
        outcomeText = message;
        outcomeLevel = level;
        RefreshStatus();
    }

    void MotorFirmware::ClearOutcome()
    {
        outcomeText.clear();
        RefreshStatus();
    }

    void MotorFirmware::RefreshStatus()
    {
        if (!outcomeText.isEmpty())
            tab.ShowStatus(outcomeText, outcomeLevel);
        else if (const auto reason = DownloadReason(); !reason.isEmpty() && !IsDownloading())
            tab.ShowStatus(reason, StatusLevel::Neutral);
        else
            tab.ShowStatus("Download sends a pack to the controller, which keeps it staged; Apply installs it later, while the motor is stopped.", StatusLevel::Neutral);
    }

    std::optional<FirmwareBoard> MotorFirmware::Board() const
    {
        if (!identity)
            return std::nullopt;
        return BoardOfKind(identity->boardKind);
    }

    std::optional<InstallerAsset> MotorFirmware::ReleasePack() const
    {
        const auto board = Board();
        if (!board || !board->hasReleasePack || !latestRelease)
            return std::nullopt;
        return FirmwarePackFor(*latestRelease, ToQt(board->name));
    }

    bool MotorFirmware::HasStagedPack() const
    {
        return identity && identity->staged != services::FirmwareVersion{};
    }

    QString MotorFirmware::CommonReason() const
    {
        if (const auto reason = commandsUnavailable(); !reason.isEmpty())
            return reason;
        if (!client.SupportsFirmwareIdentity(nodeId))
            return QString("%1 runs a firmware contract older than 2.%2, which cannot report its version or board.").arg(MotorName()).arg(CanCommandClient::firmwareContractMinor);
        if (!identity)
            return QString("Reading the firmware %1 runs…").arg(MotorName());
        if (const auto board = Board(); !board || !CanBeUpdated(*board))
            return board ? QString::fromStdString(DescribePackProblem(PackInspection{ std::nullopt, PackProblem::boardHasNoBootloader, {}, {} }, *board)) : QString("The board %1 reports is unknown to this tool.").arg(MotorName());
        if (!identity->hasUpgradeService)
            return QString("The firmware %1 runs has no firmware-upgrade service, so it cannot be updated over CAN. Install a newer firmware with a debugger.").arg(MotorName());
        if (transfer.IsActive())
            return QString("Motor %1 is busy with its firmware; one firmware transfer at a time on the bus.").arg(*transfer.ActiveNode());
        if (fetching)
            return "Downloading the release pack…";
        if (!IsStopped(motorState))
            return "Stop the motor first: firmware is downloaded and applied only while it is idle or ready. A motor in fault is cleared first.";
        return {};
    }

    QString MotorFirmware::DownloadReason() const
    {
        if (const auto reason = CommonReason(); !reason.isEmpty())
            return reason;
        if (!chosenPack && !ReleasePack())
            return "No pack to download: the latest release has none for this board. Open a pack file instead.";
        return {};
    }

    QString MotorFirmware::ApplyReason() const
    {
        if (const auto reason = CommonReason(); !reason.isEmpty())
            return reason;
        if (!HasStagedPack())
            return "No pack is staged on the controller: download one first.";
        return {};
    }

    void MotorFirmware::Refresh()
    {
        tab.ShowFirmware(RunningText(), StagedText(), ReleaseText(), PackText());

        FirmwareActions actions;
        actions.downloadReason = DownloadReason();
        actions.applyReason = ApplyReason();
        actions.download = actions.downloadReason.isEmpty();
        actions.apply = actions.applyReason.isEmpty();
        actions.discard = actions.apply;
        actions.openPack = CommonReason().isEmpty();
        actions.cancel = IsDownloading() || fetching;
        actions.tryAgain = downloadFailed && chosenPack.has_value();
        tab.SetActions(actions);
        RefreshStatus();
        emit StatusChanged();
    }

    QString MotorFirmware::RunningText() const
    {
        const auto board = Board();
        if (!identity)
            return {};
        if (!board)
            return QString("%1 on an unknown board (%2)").arg(Version(identity->running)).arg(static_cast<int>(identity->boardKind));
        return QString("%1 on the %2 (%3)").arg(Version(identity->running), ToQt(board->name), ToQt(board->microcontroller));
    }

    QString MotorFirmware::StagedText() const
    {
        if (!identity)
            return {};
        if (!identity->hasUpgradeService)
            return "not reported by this firmware";
        return HasStagedPack() ? QString("%1, apply when ready").arg(Version(identity->staged)) : QString("none");
    }

    QString MotorFirmware::ReleaseText() const
    {
        if (!latestRelease)
            return "not checked yet (Help, Check for Updates)";

        const auto version = ToQt(latestRelease->version.ToString());
        if (!ReleasePack())
            return QString("%1, with no pack for this board").arg(version);
        return UpdateAvailable() ? QString("%1, newer than the running firmware").arg(version) : QString("%1, the running firmware is current").arg(version);
    }

    QString MotorFirmware::PackText() const
    {
        if (chosenPack)
            return QString("%1, firmware %2, %3").arg(chosenSource, Version(chosenPack->version), Kilobytes(static_cast<qint64>(chosenPack->bytes.size())));
        if (const auto pack = ReleasePack())
            return QString("%1 from the release, downloaded when you start").arg(pack->fileName);
        return {};
    }

    QString MotorFirmware::MotorName() const
    {
        return QString("Motor %1").arg(nodeId);
    }

    void MotorFirmware::Report(IssueSeverity severity, const QString& message) const
    {
        reporter(severity, QString("firmware: %1").arg(message));
    }
}
