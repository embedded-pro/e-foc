#include "tools/can_commander/logic/FirmwareTransfer.hpp"
#include <algorithm>

namespace tool
{
    using services::FwuError;

    FirmwareTransfer::FirmwareTransfer(CanCommandClient& client)
        : services::FirmwareUpgradeCategoryClientObserver(client.FirmwareUpgrade())
        , client(client)
        , category(client.FirmwareUpgrade())
    {}

    bool FirmwareTransfer::IsActive() const
    {
        return phase != TransferPhase::idle;
    }

    std::optional<uint16_t> FirmwareTransfer::ActiveNode() const
    {
        if (!IsActive())
            return std::nullopt;
        return nodeId;
    }

    TransferPhase FirmwareTransfer::Phase() const
    {
        return phase;
    }

    bool FirmwareTransfer::StartDownload(uint16_t node, const InspectedPack& inspected)
    {
        if (IsActive())
            return false;

        nodeId = node;
        pack = inspected.bytes;
        crc32 = inspected.crc32;
        nextBlock = 0;
        failures = 0;
        Enter(TransferPhase::erasing);
        ReportProgress();

        if (!category.SendBeginUpgrade(nodeId, static_cast<uint32_t>(pack.size())))
        {
            FinishDownload(DownloadOutcome::notSent);
            return true;
        }

        answerTimer.Start(beginTimeout, [this]()
            {
                GiveUp();
            });
        return true;
    }

    void FirmwareTransfer::Cancel()
    {
        if (!Downloading())
            return;

        StopTimers();
        Enter(TransferPhase::cancelling);
        category.SendAbort(nodeId);
        answerTimer.Start(answerTimeout, [this]()
            {
                FinishDownload(DownloadOutcome::cancelled);
            });
    }

    bool FirmwareTransfer::Discard(uint16_t node)
    {
        if (IsActive())
            return false;

        nodeId = node;
        Enter(TransferPhase::discarding);
        if (!category.SendAbort(nodeId))
        {
            FinishDiscard(false);
            return true;
        }

        answerTimer.Start(answerTimeout, [this]()
            {
                FinishDiscard(false);
            });
        return true;
    }

    bool FirmwareTransfer::Apply(uint16_t node, const services::FirmwareVersion& staged)
    {
        if (IsActive())
            return false;

        nodeId = node;
        expected = staged;
        Enter(TransferPhase::activating);
        if (!category.SendActivate(nodeId))
        {
            FinishApply(ApplyOutcome::notSent);
            return true;
        }

        answerTimer.Start(answerTimeout, [this]()
            {
                FinishApply(ApplyOutcome::noAnswer);
            });
        return true;
    }

    void FirmwareTransfer::Abandon()
    {
        if (!IsActive())
            return;

        StopTimers();
        Enter(TransferPhase::idle);
        NotifyObservers([node = nodeId](auto& observer)
            {
                observer.OnTransferAbandoned(node);
            });
    }

    void FirmwareTransfer::EmergencyStopSent(uint16_t node)
    {
        if (node == nodeId && Downloading())
            FinishDownload(DownloadOutcome::stopped);
    }

    void FirmwareTransfer::OnCommandAck(uint16_t node, uint8_t categoryId, uint8_t commandType, services::CanAckStatus status)
    {
        if (node != nodeId || categoryId != services::firmwareUpgradeCategoryId || commandType != services::fwuAbortId)
            return;

        if (phase == TransferPhase::cancelling)
            FinishDownload(DownloadOutcome::cancelled);
        else if (phase == TransferPhase::discarding)
            FinishDiscard(status == services::CanAckStatus::success);
    }

    void FirmwareTransfer::OnFirmwareIdentity(uint16_t node, const FirmwareIdentity& identity)
    {
        if (phase != TransferPhase::restarting || node != nodeId || identity.staged != services::FirmwareVersion{})
            return;

        FinishApply(identity.running == expected ? ApplyOutcome::updated : ApplyOutcome::oldVersionKept, FwuError::ok, identity.running);
    }

    void FirmwareTransfer::OnBeginResponse(FwuError status, uint16_t)
    {
        if (!Answers(TransferPhase::erasing))
            return;

        answerTimer.Cancel();
        if (status != FwuError::ok)
        {
            FinishDownload(DownloadOutcome::refused, status);
            return;
        }

        Enter(TransferPhase::sending);
        SendNextBlock();
    }

    void FirmwareTransfer::OnDataBlockAck(FwuError status, uint16_t blockIndex)
    {
        if (!Answers(TransferPhase::sending) || (status == FwuError::ok && blockIndex != nextBlock))
            return;

        answerTimer.Cancel();
        if (status == FwuError::sequenceError || status == FwuError::busy)
            OnNoBlockAnswer();
        else if (status != FwuError::ok)
            FinishDownload(DownloadOutcome::refused, status);
        else
        {
            failures = 0;
            ++nextBlock;
            SendNextBlock();
        }
    }

    void FirmwareTransfer::OnVerifyResponse(FwuError status)
    {
        if (!Answers(TransferPhase::verifying))
            return;

        answerTimer.Cancel();
        if (status == FwuError::ok)
            FinishDownload(DownloadOutcome::staged);
        else if (status == FwuError::busy)
            RetryVerify();
        else
            FinishDownload(DownloadOutcome::refused, status);
    }

    void FirmwareTransfer::OnActivateResponse(FwuError status)
    {
        if (!Answers(TransferPhase::activating))
            return;

        answerTimer.Cancel();
        if (status != FwuError::ok)
        {
            FinishApply(ApplyOutcome::refused, status);
            return;
        }

        Enter(TransferPhase::restarting);
        ReportProgress();
        restartTimer.Start(restartTimeout, [this]()
            {
                FinishApply(ApplyOutcome::noNode);
            });
        identityTimer.Start(identityRetry, [this]()
            {
                client.RequestFirmwareIdentity(nodeId);
            });
    }

    void FirmwareTransfer::OnProgressResponse(services::FwuState state, uint16_t blocksReceived, uint16_t)
    {
        if (!Answers(TransferPhase::sending))
            return;

        answerTimer.Cancel();
        if (state != services::FwuState::receiving)
        {
            FinishDownload(DownloadOutcome::refused, FwuError::invalidState);
            return;
        }

        nextBlock = std::min(blocksReceived, NumberOfBlocks());
        SendNextBlock();
    }

    bool FirmwareTransfer::Answers(TransferPhase expectedPhase) const
    {
        return phase == expectedPhase && client.FrameOrigin().SourceNodeId() == nodeId;
    }

    bool FirmwareTransfer::Downloading() const
    {
        return phase == TransferPhase::erasing || phase == TransferPhase::sending || phase == TransferPhase::verifying;
    }

    void FirmwareTransfer::Enter(TransferPhase next)
    {
        phase = next;
        if (next == TransferPhase::idle)
            pack.clear();
    }

    void FirmwareTransfer::ReportProgress()
    {
        const TransferProgress progress{ phase, BytesSent(), static_cast<uint32_t>(pack.size()) };
        NotifyObservers([node = nodeId, progress](auto& observer)
            {
                observer.OnTransferProgress(node, progress);
            });
    }

    uint16_t FirmwareTransfer::NumberOfBlocks() const
    {
        return static_cast<uint16_t>((pack.size() + blockSize - 1) / blockSize);
    }

    uint32_t FirmwareTransfer::BytesSent() const
    {
        return static_cast<uint32_t>(std::min(static_cast<std::size_t>(nextBlock) * blockSize, pack.size()));
    }

    void FirmwareTransfer::SendNextBlock()
    {
        if (nextBlock == NumberOfBlocks())
        {
            SendVerify();
            return;
        }

        ReportProgress();
        const auto begin = static_cast<std::size_t>(nextBlock) * blockSize;
        hal::Can::Message data;
        for (auto index = begin; index != std::min(begin + blockSize, pack.size()); ++index)
            data.push_back(pack[index]);

        if (!category.SendDataBlock(nodeId, nextBlock, data))
        {
            FinishDownload(DownloadOutcome::notSent);
            return;
        }

        answerTimer.Start(blockTimeout, [this]()
            {
                OnNoBlockAnswer();
            });
    }

    void FirmwareTransfer::SendVerify()
    {
        Enter(TransferPhase::verifying);
        ReportProgress();
        if (!category.SendVerify(nodeId, crc32))
        {
            FinishDownload(DownloadOutcome::notSent);
            return;
        }

        answerTimer.Start(verifyTimeout, [this]()
            {
                GiveUp();
            });
    }

    void FirmwareTransfer::OnNoBlockAnswer()
    {
        if (++failures >= attempts)
            GiveUp();
        else
            AskProgress();
    }

    void FirmwareTransfer::RetryVerify()
    {
        if (++failures >= attempts)
            GiveUp();
        else
            SendVerify();
    }

    void FirmwareTransfer::AskProgress()
    {
        if (!category.SendQueryProgress(nodeId))
        {
            FinishDownload(DownloadOutcome::notSent);
            return;
        }

        answerTimer.Start(blockTimeout, [this]()
            {
                OnNoBlockAnswer();
            });
    }

    void FirmwareTransfer::GiveUp()
    {
        category.SendAbort(nodeId);
        FinishDownload(DownloadOutcome::noAnswer);
    }

    void FirmwareTransfer::FinishDownload(DownloadOutcome outcome, FwuError error)
    {
        StopTimers();
        Enter(TransferPhase::idle);
        NotifyObservers([node = nodeId, outcome, error](auto& observer)
            {
                observer.OnDownloadFinished(node, outcome, error);
            });
    }

    void FirmwareTransfer::FinishDiscard(bool acknowledged)
    {
        StopTimers();
        Enter(TransferPhase::idle);
        NotifyObservers([node = nodeId, acknowledged](auto& observer)
            {
                observer.OnDiscardFinished(node, acknowledged);
            });
    }

    void FirmwareTransfer::FinishApply(ApplyOutcome outcome, FwuError error, const services::FirmwareVersion& running)
    {
        StopTimers();
        Enter(TransferPhase::idle);
        NotifyObservers([node = nodeId, outcome, error, running](auto& observer)
            {
                observer.OnApplyFinished(node, outcome, error, running);
            });
    }

    void FirmwareTransfer::StopTimers()
    {
        answerTimer.Cancel();
        restartTimer.Cancel();
        identityTimer.Cancel();
    }

    std::optional<std::chrono::seconds> TimeLeft(uint32_t bytesSent, uint32_t totalBytes, std::chrono::milliseconds elapsed)
    {
        if (bytesSent == 0 || elapsed.count() <= 0)
            return std::nullopt;

        const auto remaining = static_cast<int64_t>(totalBytes) - static_cast<int64_t>(bytesSent);
        const auto milliseconds = remaining * elapsed.count() / static_cast<int64_t>(bytesSent);
        return std::chrono::seconds{ (milliseconds + 999) / 1000 };
    }
}
