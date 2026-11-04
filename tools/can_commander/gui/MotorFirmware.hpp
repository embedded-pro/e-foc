#pragma once

#include "tools/can_commander/gui/FirmwareTab.hpp"
#include "tools/can_commander/logic/CanCommandClient.hpp"
#include "tools/can_commander/logic/FirmwarePack.hpp"
#include "tools/can_commander/logic/FirmwareTransfer.hpp"
#include "tools/can_commander/support/IssueLog.hpp"
#include "tools/can_commander/support/ReleaseFeed.hpp"
#include "tools/can_commander/support/UpdateDownloader.hpp"
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QWidget>
#include <functional>
#include <optional>
#include <vector>

namespace tool
{
    class MotorFirmware
        : public QObject
    {
        Q_OBJECT

    public:
        using Reporter = std::function<void(IssueSeverity, const QString&)>;
        using Reason = std::function<QString()>;

        MotorFirmware(CanCommandClient& client, FirmwareTransfer& transfer, uint16_t nodeId, FirmwareTab& tab, Reporter reporter, Reason commandsUnavailable, QWidget* dialogParent);

        bool UpdateAvailable() const;
        QString UpdateSummary() const;
        bool IsDownloading() const;

        void SetLatestRelease(const ReleaseInfo& release);
        bool OfferPackFile(const std::vector<uint8_t>& bytes, const QString& source);
        void SetMotorState(std::optional<FocMotorState> state);
        void OnFirmwareIdentity(const FirmwareIdentity& identity);
        void ForgetNodeState();
        void Refresh();

        void OnTransferProgress(const TransferProgress& progress);
        void OnDownloadFinished(DownloadOutcome outcome, services::FwuError error);
        void OnDiscardFinished(bool acknowledged);
        void OnApplyFinished(ApplyOutcome outcome, services::FwuError error, const services::FirmwareVersion& running);
        void OnTransferAbandoned();

    signals:
        void StatusChanged();
        void TransferChanged();

    private:
        void ConnectTab();
        void ConnectDownloader();
        void OpenPack();
        void Download();
        void FetchReleasePack(const InstallerAsset& asset);
        void OnReleasePackFetched(const QString& path);
        bool Offer(const std::vector<uint8_t>& bytes, const QString& source, bool fromRelease);
        bool ConfirmDownload();
        void StartTransfer();
        void Apply();
        void Discard();
        void Fail(const QString& message);
        void FailDownload(const QString& message);
        void Succeed(const QString& message, IssueSeverity severity = IssueSeverity::info);
        void ShowOutcome(const QString& message, ui::theme::StatusLevel level);
        void ClearOutcome();
        void RefreshStatus();

        std::optional<FirmwareBoard> Board() const;
        std::optional<InstallerAsset> ReleasePack() const;
        bool HasStagedPack() const;
        QString CommonReason() const;
        QString DownloadReason() const;
        QString ApplyReason() const;
        QString RunningText() const;
        QString StagedText() const;
        QString ReleaseText() const;
        QString PackText() const;
        QString MotorName() const;
        void Report(IssueSeverity severity, const QString& message) const;

        CanCommandClient& client;
        FirmwareTransfer& transfer;
        const uint16_t nodeId;
        FirmwareTab& tab;
        Reporter reporter;
        Reason commandsUnavailable;
        QWidget* dialogParent;
        UpdateDownloader downloader;

        std::optional<FirmwareIdentity> identity;
        std::optional<ReleaseInfo> latestRelease;
        std::optional<FocMotorState> motorState;
        std::optional<InspectedPack> chosenPack;
        QString chosenSource;
        bool fetching{ false };
        bool downloadFailed{ false };
        QString outcomeText;
        ui::theme::StatusLevel outcomeLevel{ ui::theme::StatusLevel::Neutral };
        QElapsedTimer transferClock;
    };
}
