#include "core/services/firmware_upgrade/FirmwareUpgrade.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    namespace
    {
        template<class F>
        F Take(F& function)
        {
            auto taken = function;
            function = nullptr;
            return taken;
        }
    }

    FirmwareUpgrade::FirmwareUpgrade(FirmwareUpgradeCategoryServer& server, const std::optional<application::FirmwareStaging>& staging,
        FirmwareUpgradeDrive& drive, const infra::Function<void()>& reset)
        : FirmwareUpgradeCategoryServerObserver(server)
        , staging(staging)
        , drive(drive)
        , reset(reset)
    {
        if (!this->staging)
            return;

        really_assert(this->staging->pageSize >= blockSize && this->staging->pageSize <= maxPageSize);
        journal.emplace(this->staging->journal);
        writer.emplace(this->staging->pack, this->staging->pageSize, infra::MakeRange(scratch), [this]()
            {
                return !TakeAbort();
            });
        reader.emplace(this->staging->pack, infra::MakeRange(scratch), [this]()
            {
                return !TakeAbort();
            });

        if (journal->Status() == application::UpgradePackStatus::downloaded)
        {
            state = State::staged;
            ReadStagedVersion();
        }
    }

    void FirmwareUpgrade::OnBeginUpgrade(uint32_t firmwareSize, const infra::Function<void(FwuError, uint16_t)>& onResult)
    {
        if (auto refusal = BeginRefusal(firmwareSize))
        {
            onResult(*refusal, 0);
            return;
        }

        beginDone = onResult;
        StartDownload(firmwareSize);
    }

    void FirmwareUpgrade::OnDataBlock(uint16_t blockIndex, infra::ConstByteRange data, const infra::Function<void(FwuError)>& onResult)
    {
        if (state != State::receiving)
            onResult(FwuError::invalidState);
        else if (FlashBusy())
            onResult(FwuError::busy);
        else if (nextBlock != 0 && blockIndex == nextBlock - 1)
        {
            RestartDeadline();
            onResult(FwuError::ok);
        }
        else if (blockIndex != nextBlock)
            onResult(FwuError::sequenceError);
        else if (data.size() > packSize - writer->Written())
            onResult(FwuError::invalidSize);
        else
        {
            RestartDeadline();
            blockDone = onResult;
            ++nextBlock;
            writer->Append(data, [this](bool stored)
                {
                    OnBlockStored(stored);
                });
        }
    }

    void FirmwareUpgrade::OnVerify(uint32_t expectedCrc32, const infra::Function<void(FwuError)>& onResult)
    {
        if (state != State::receiving)
            onResult(FwuError::invalidState);
        else if (FlashBusy())
            onResult(FwuError::busy);
        else if (writer->Written() != packSize)
            onResult(FwuError::invalidSize);
        else
        {
            deadline.Cancel();
            verifyDone = onResult;
            Enter(State::verifying);
            if (writer->HoldsPartialPage())
                writer->Flush([this, expectedCrc32](bool flushed)
                    {
                        if (flushed)
                            CheckCrc(expectedCrc32);
                        else
                            Fail(FwuError::writeError);
                    });
            else
                CheckCrc(expectedCrc32);
        }
    }

    void FirmwareUpgrade::OnActivate(const infra::Function<void(FwuError)>& onResult)
    {
        if (state != State::staged)
        {
            onResult(FwuError::notReady);
            return;
        }

        if (FlashBusy() || !drive.IsStoppedAndIdle())
        {
            onResult(FwuError::busy);
            return;
        }

        journal->Record(application::UpgradePackStatus::readyToDeploy, 0);
        Enter(State::restarting);
        onResult(FwuError::ok);
        restartTimer.Start(restartDelay, [this]()
            {
                reset();
            });
    }

    void FirmwareUpgrade::OnAbort(const infra::Function<void()>& onDone)
    {
        if (FlashBusy())
        {
            abortRequested = true;
            abortDone = onDone;
            return;
        }

        DoAbort();
        onDone();
    }

    void FirmwareUpgrade::OnQueryProgress(const infra::Function<void(FwuState, uint16_t, uint16_t)>& onResult)
    {
        const auto totalBlocks = static_cast<uint16_t>((packSize + blockSize - 1) / blockSize);
        onResult(ReportedState(), nextBlock, totalBlocks);
    }

    void FirmwareUpgrade::OnSessionTimeout()
    {
        OnDeadline();
    }

    bool FirmwareUpgrade::IsRunning() const
    {
        return Holds(state);
    }

    FirmwareVersion FirmwareUpgrade::StagedVersion() const
    {
        return stagedVersion;
    }

    void FirmwareUpgrade::EmergencyStop()
    {
        if (!Downloading())
            return;

        if (FlashBusy())
            abortRequested = true;
        else
            EndDownload();
    }

    bool FirmwareUpgrade::Holds(State state)
    {
        return state == State::erasing || state == State::receiving || state == State::verifying || state == State::restarting;
    }

    void FirmwareUpgrade::Enter(State next)
    {
        const bool wasHeld = Holds(state);
        state = next;

        if (Holds(next) && !wasHeld)
            drive.Hold();
        else if (!Holds(next) && wasHeld)
            drive.Release();
    }

    bool FirmwareUpgrade::Downloading() const
    {
        return state == State::erasing || state == State::receiving || state == State::verifying;
    }

    std::optional<FwuError> FirmwareUpgrade::BeginRefusal(uint32_t firmwareSize) const
    {
        if (!staging)
            return FwuError::invalidSize;

        if (Downloading() || state == State::restarting || FlashBusy() || !drive.IsStoppedAndIdle())
            return FwuError::busy;

        if (firmwareSize < minimumPackSize || firmwareSize > staging->pack.TotalSize())
            return FwuError::invalidSize;

        return std::nullopt;
    }

    void FirmwareUpgrade::StartDownload(uint32_t firmwareSize)
    {
        discardTimer.Cancel();
        packSize = firmwareSize;
        nextBlock = 0;
        stagedVersion = {};
        Enter(State::erasing);

        if (journal->Status() != application::UpgradePackStatus::empty)
            journal->Clear();

        writer->Erase([this](bool erased)
            {
                OnSlotErased(erased);
            });
    }

    void FirmwareUpgrade::OnSlotErased(bool erased)
    {
        if (!erased)
        {
            Fail(FwuError::writeError);
            return;
        }

        Enter(State::receiving);
        RestartDeadline();
        Take(beginDone)(FwuError::ok, staging->pageSize);
    }

    void FirmwareUpgrade::OnBlockStored(bool stored)
    {
        if (stored)
            Take(blockDone)(FwuError::ok);
        else
            Fail(FwuError::writeError);
    }

    void FirmwareUpgrade::CheckCrc(uint32_t expectedCrc)
    {
        reader->CheckCrc(packSize, expectedCrc, [this](bool matches)
            {
                if (!matches)
                    Fail(FwuError::crcMismatch);
                else
                    reader->ReadHeader(packSize, [this](const PackHeader& header)
                        {
                            OnVerifiedHeader(header);
                        });
            });
    }

    void FirmwareUpgrade::OnVerifiedHeader(const PackHeader& header)
    {
        const auto version = CheckPackHeader(header, PackLimits{ staging->productName, staging->applicationCapacity, packSize });
        if (!version)
        {
            Fail(FwuError::notReady);
            return;
        }

        journal->Record(application::UpgradePackStatus::downloaded, 0);
        stagedVersion = *version;
        Enter(State::staged);
        Take(verifyDone)(FwuError::ok);
    }

    void FirmwareUpgrade::ReadStagedVersion()
    {
        reader->ReadHeader(staging->pack.TotalSize(), [this](const PackHeader& header)
            {
                stagedVersion = ParseVersion(header.epilogue.productVersion).value_or(FirmwareVersion{});
            });
    }

    bool FirmwareUpgrade::FlashBusy() const
    {
        return (writer && writer->Busy()) || (reader && reader->Busy());
    }

    bool FirmwareUpgrade::TakeAbort()
    {
        if (!abortRequested)
            return false;

        abortRequested = false;
        auto done = Take(abortDone);
        DoAbort();
        if (done != nullptr)
            done();
        return true;
    }

    void FirmwareUpgrade::DoAbort()
    {
        if (Downloading())
            EndDownload();
        else if (state == State::staged)
            Discard();
        else if (state == State::error)
            Enter(State::idle);
    }

    void FirmwareUpgrade::Fail(FwuError result)
    {
        deadline.Cancel();
        Enter(State::error);
        AnswerPending(result);
    }

    void FirmwareUpgrade::EndDownload()
    {
        deadline.Cancel();
        Enter(State::idle);
        AnswerPending(FwuError::invalidState);
    }

    void FirmwareUpgrade::AnswerPending(FwuError result)
    {
        if (beginDone != nullptr)
            Take(beginDone)(result, 0);
        if (blockDone != nullptr)
            Take(blockDone)(result);
        if (verifyDone != nullptr)
            Take(verifyDone)(result);
    }

    void FirmwareUpgrade::Discard()
    {
        Enter(State::idle);
        stagedVersion = {};
        TryDiscard();
    }

    void FirmwareUpgrade::TryDiscard()
    {
        if (!drive.IsStoppedAndIdle())
        {
            if (!discardTimer.Armed())
                discardTimer.Start(discardRetry, [this]()
                    {
                        TryDiscard();
                    });
            return;
        }

        discardTimer.Cancel();
        journal->Clear();
    }

    void FirmwareUpgrade::RestartDeadline()
    {
        deadline.Start(inactivityDeadline, [this]()
            {
                OnDeadline();
            });
    }

    void FirmwareUpgrade::OnDeadline()
    {
        if (state != State::receiving)
            return;

        if (FlashBusy())
            abortRequested = true;
        else
            EndDownload();
    }

    FwuState FirmwareUpgrade::ReportedState() const
    {
        switch (state)
        {
            case State::erasing:
            case State::receiving:
                return FwuState::receiving;
            case State::verifying:
                return FwuState::verifying;
            case State::staged:
            case State::restarting:
                return FwuState::complete;
            case State::error:
                return FwuState::error;
            default:
                return FwuState::idle;
        }
    }
}
