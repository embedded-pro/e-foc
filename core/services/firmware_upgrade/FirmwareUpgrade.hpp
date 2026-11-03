#pragma once

#include "bootloader/StatusJournal.hpp"
#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "core/platform_abstraction/FirmwareStaging.hpp"
#include "core/services/firmware_upgrade/FirmwareUpgradeControl.hpp"
#include "core/services/firmware_upgrade/FirmwareUpgradeDrive.hpp"
#include "core/services/firmware_upgrade/PackHeaderCheck.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Crc.hpp"
#include "infra/util/Function.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

namespace services
{
    class FirmwareUpgrade
        : public FirmwareUpgradeCategoryServerObserver
        , public FirmwareUpgradeControl
    {
    public:
        static constexpr uint16_t maxPageSize{ 256 };
        static constexpr uint8_t blockSize{ 6 };
        static constexpr infra::Duration inactivityDeadline{ std::chrono::seconds(30) };
        static constexpr infra::Duration restartDelay{ std::chrono::milliseconds(250) };
        static constexpr infra::Duration discardRetry{ std::chrono::milliseconds(100) };

        FirmwareUpgrade(FirmwareUpgradeCategoryServer& server, const std::optional<application::FirmwareStaging>& staging,
            FirmwareUpgradeDrive& drive, const infra::Function<void()>& reset);

        void OnBeginUpgrade(uint32_t firmwareSize, const infra::Function<void(FwuError, uint16_t)>& onResult) override;
        void OnDataBlock(uint16_t blockIndex, infra::ConstByteRange data, const infra::Function<void(FwuError)>& onResult) override;
        void OnVerify(uint32_t expectedCrc32, const infra::Function<void(FwuError)>& onResult) override;
        void OnActivate(const infra::Function<void(FwuError)>& onResult) override;
        void OnAbort(const infra::Function<void()>& onDone) override;
        void OnQueryProgress(const infra::Function<void(FwuState, uint16_t, uint16_t)>& onResult) override;
        void OnSessionTimeout() override;

        bool IsRunning() const override;
        FirmwareVersion StagedVersion() const override;
        void EmergencyStop() override;

    private:
        enum class State : uint8_t
        {
            idle,
            erasing,
            receiving,
            verifying,
            staged,
            error,
            restarting
        };

        static bool Holds(State state);
        void Enter(State next);
        bool Downloading() const;
        std::optional<FwuError> BeginRefusal(uint32_t firmwareSize) const;
        void StartDownload(uint32_t firmwareSize);

        void EraseNextSector();
        void OnSectorErased();
        void CheckErasedChunk();
        void OnErasedChunk(uint32_t length);
        void OnSlotErased();

        void AcceptBlock(infra::ConstByteRange data);
        void ProgramPage();
        void OnPageWritten();
        void OnPageProgrammed();
        void CompleteBlock();

        void StartCrc();
        void ReadNextCrcChunk();
        void OnCrcChunk(uint32_t length);
        void ReadHeader(const infra::Function<void()>& onRead);
        void ReadEpilogue();
        void ReadImagePrologue();
        void FinishHeader();
        uint32_t HeaderReadLimit() const;
        void OnVerifiedHeader();
        void ReadStagedVersion();

        bool FlashDone();
        bool TakeAbort();
        void DoAbort();
        void Fail(FwuError result);
        void EndDownload();
        void AnswerPending(FwuError result);
        void Discard();
        void TryDiscard();
        void RestartDeadline();
        void OnDeadline();
        FwuState ReportedState() const;

        std::optional<application::FirmwareStaging> staging;
        FirmwareUpgradeDrive& drive;
        infra::Function<void()> reset;
        std::optional<bootloader::StatusJournal> journal;

        State state{ State::idle };
        bool flashBusy{ false };
        bool abortRequested{ false };
        infra::Function<void()> abortDone;

        uint32_t packSize{ 0 };
        uint32_t received{ 0 };
        uint16_t nextBlock{ 0 };
        uint32_t eraseSector{ 0 };
        uint32_t readAddress{ 0 };
        uint32_t readEnd{ 0 };
        uint32_t pageAddress{ 0 };
        uint16_t pageFill{ 0 };
        std::array<uint8_t, maxPageSize> page{};
        std::array<uint8_t, maxPageSize> readBack{};
        std::array<uint8_t, blockSize> carry{};
        uint8_t carryFill{ 0 };

        uint32_t expectedCrc{ 0 };
        infra::Crc32 crc;
        PackHeader header{};
        infra::Function<void()> onHeaderRead;
        FirmwareVersion stagedVersion{};

        infra::Function<void(FwuError, uint16_t)> beginDone;
        infra::Function<void(FwuError)> blockDone;
        infra::Function<void(FwuError)> verifyDone;

        infra::TimerSingleShot deadline;
        infra::TimerSingleShot restartTimer;
        infra::TimerRepeating discardTimer;
    };
}
