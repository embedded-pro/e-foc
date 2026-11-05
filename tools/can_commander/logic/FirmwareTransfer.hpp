#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryClient.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Observer.hpp"
#include "tools/can_commander/logic/CanCommandClient.hpp"
#include "tools/can_commander/logic/FirmwarePack.hpp"
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace tool
{
    class FirmwareTransfer;

    enum class TransferPhase : uint8_t
    {
        idle,
        erasing,
        sending,
        verifying,
        cancelling,
        discarding,
        activating,
        restarting
    };

    enum class DownloadOutcome : uint8_t
    {
        staged,
        cancelled,
        stopped,
        refused,
        noAnswer,
        notSent
    };

    enum class ApplyOutcome : uint8_t
    {
        updated,
        oldVersionKept,
        noNode,
        refused,
        noAnswer,
        notSent
    };

    struct TransferProgress
    {
        TransferPhase phase{ TransferPhase::idle };
        uint32_t bytesSent{ 0 };
        uint32_t totalBytes{ 0 };

        bool operator==(const TransferProgress& other) const = default;
    };

    class FirmwareTransferObserver
        : public infra::SingleObserver<FirmwareTransferObserver, FirmwareTransfer>
    {
    public:
        using infra::SingleObserver<FirmwareTransferObserver, FirmwareTransfer>::SingleObserver;

        virtual void OnTransferProgress(uint16_t nodeId, const TransferProgress& progress) = 0;
        virtual void OnDownloadFinished(uint16_t nodeId, DownloadOutcome outcome, services::FwuError error) = 0;
        virtual void OnDiscardFinished(uint16_t nodeId, bool acknowledged) = 0;
        virtual void OnApplyFinished(uint16_t nodeId, ApplyOutcome outcome, services::FwuError error, const services::FirmwareVersion& running) = 0;
        virtual void OnTransferAbandoned(uint16_t nodeId) = 0;
    };

    class FirmwareTransfer
        : public infra::Subject<FirmwareTransferObserver>
        , private services::FirmwareUpgradeCategoryClientObserver
    {
    public:
        static constexpr std::size_t blockSize{ 6 };
        static constexpr uint8_t attempts{ 3 };
        static constexpr infra::Duration beginTimeout{ std::chrono::seconds(10) };
        static constexpr infra::Duration blockTimeout{ std::chrono::seconds(1) };
        static constexpr infra::Duration verifyTimeout{ std::chrono::seconds(10) };
        static constexpr infra::Duration answerTimeout{ std::chrono::seconds(2) };
        static constexpr infra::Duration restartTimeout{ std::chrono::seconds(30) };
        static constexpr infra::Duration identityRetry{ std::chrono::seconds(5) };

        explicit FirmwareTransfer(CanCommandClient& client);

        bool IsActive() const;
        std::optional<uint16_t> ActiveNode() const;
        TransferPhase Phase() const;

        bool StartDownload(uint16_t nodeId, const InspectedPack& pack);
        void Cancel();
        bool Discard(uint16_t nodeId);
        bool Apply(uint16_t nodeId, const services::FirmwareVersion& staged);
        void Abandon();
        void EmergencyStopSent(uint16_t nodeId);

        void OnCommandAck(uint16_t nodeId, uint8_t categoryId, uint8_t commandType, services::CanAckStatus status);
        void OnFirmwareIdentity(uint16_t nodeId, const FirmwareIdentity& identity);

    private:
        void OnBeginResponse(services::FwuError status, uint16_t pageSize) override;
        void OnDataBlockAck(services::FwuError status, uint16_t blockIndex) override;
        void OnVerifyResponse(services::FwuError status) override;
        void OnActivateResponse(services::FwuError status) override;
        void OnProgressResponse(services::FwuState state, uint16_t blocksReceived, uint16_t totalBlocks) override;

        bool Answers(TransferPhase expected) const;
        bool Downloading() const;
        void Enter(TransferPhase next);
        void ReportProgress();
        uint16_t NumberOfBlocks() const;
        uint32_t BytesSent() const;
        void SendNextBlock();
        void SendVerify();
        void OnNoBlockAnswer();
        void RetryVerify();
        void AskProgress();
        void GiveUp();
        void FinishDownload(DownloadOutcome outcome, services::FwuError error = services::FwuError::ok);
        void FinishDiscard(bool acknowledged);
        void FinishApply(ApplyOutcome outcome, services::FwuError error = services::FwuError::ok, const services::FirmwareVersion& running = {});
        void StopTimers();

        CanCommandClient& client;
        services::FirmwareUpgradeCategoryClient& category;
        TransferPhase phase{ TransferPhase::idle };
        uint16_t nodeId{ 0 };
        std::vector<uint8_t> pack;
        uint32_t crc32{ 0 };
        uint16_t nextBlock{ 0 };
        uint8_t failures{ 0 };
        services::FirmwareVersion expected;
        infra::TimerSingleShot answerTimer;
        infra::TimerSingleShot restartTimer;
        infra::TimerRepeating identityTimer;
    };

    std::optional<std::chrono::seconds> TimeLeft(uint32_t bytesSent, uint32_t totalBytes, std::chrono::milliseconds elapsed);
}
