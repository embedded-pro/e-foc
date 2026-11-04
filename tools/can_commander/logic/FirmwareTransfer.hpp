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
        cancelling
    };

    enum class DownloadOutcome : uint8_t
    {
        staged,
        cancelled,
        refused,
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
    };

    class FirmwareTransfer
        : public infra::Subject<FirmwareTransferObserver>
        , private services::FirmwareUpgradeCategoryClientObserver
    {
    public:
        static constexpr std::size_t blockSize{ 6 };
        static constexpr infra::Duration answerTimeout{ std::chrono::seconds(2) };

        explicit FirmwareTransfer(CanCommandClient& client);

        bool IsActive() const;
        std::optional<uint16_t> ActiveNode() const;
        TransferPhase Phase() const;

        bool StartDownload(uint16_t nodeId, const InspectedPack& pack);
        void Cancel();

        void OnCommandAck(uint16_t nodeId, uint8_t categoryId, uint8_t commandType, services::CanAckStatus status);

    private:
        void OnBeginResponse(services::FwuError status, uint16_t pageSize) override;
        void OnDataBlockAck(services::FwuError status, uint16_t blockIndex) override;
        void OnVerifyResponse(services::FwuError status) override;
        void OnActivateResponse(services::FwuError status) override;
        void OnProgressResponse(services::FwuState state, uint16_t blocksReceived, uint16_t totalBlocks) override;

        bool Answers(TransferPhase expected) const;
        void Enter(TransferPhase next);
        void ReportProgress();
        uint16_t NumberOfBlocks() const;
        uint32_t BytesSent() const;
        void SendNextBlock();
        void SendVerify();
        void FinishDownload(DownloadOutcome outcome, services::FwuError error = services::FwuError::ok);
        void StopTimers();

        CanCommandClient& client;
        services::FirmwareUpgradeCategoryClient& category;
        TransferPhase phase{ TransferPhase::idle };
        uint16_t nodeId{ 0 };
        std::vector<uint8_t> pack;
        uint32_t crc32{ 0 };
        uint16_t nextBlock{ 0 };
        infra::TimerSingleShot answerTimer;
    };

    std::optional<std::chrono::seconds> TimeLeft(uint32_t bytesSent, uint32_t totalBytes, std::chrono::milliseconds elapsed);
}
