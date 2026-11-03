#include "core/services/firmware_upgrade/FirmwareUpgrade.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

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
        FirmwareUpgradeDrive& drive)
        : FirmwareUpgradeCategoryServerObserver(server)
        , staging(staging)
        , drive(drive)
    {
        if (!this->staging)
            return;

        really_assert(this->staging->pageSize >= blockSize && this->staging->pageSize <= maxPageSize);
        journal.emplace(this->staging->journal);
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
        else if (flashBusy)
            onResult(FwuError::busy);
        else if (nextBlock != 0 && blockIndex == nextBlock - 1)
        {
            RestartDeadline();
            onResult(FwuError::ok);
        }
        else if (blockIndex != nextBlock)
            onResult(FwuError::sequenceError);
        else if (data.size() > packSize - received)
            onResult(FwuError::invalidSize);
        else
        {
            RestartDeadline();
            blockDone = onResult;
            ++nextBlock;
            AcceptBlock(data);
        }
    }

    void FirmwareUpgrade::OnVerify(uint32_t expectedCrc32, const infra::Function<void(FwuError)>& onResult)
    {
        if (state != State::receiving)
            onResult(FwuError::invalidState);
        else if (flashBusy)
            onResult(FwuError::busy);
        else if (received != packSize)
            onResult(FwuError::invalidSize);
        else
        {
            deadline.Cancel();
            expectedCrc = expectedCrc32;
            verifyDone = onResult;
            Enter(State::verifying);
            if (pageFill != 0)
                ProgramPage();
            else
                StartCrc();
        }
    }

    void FirmwareUpgrade::OnActivate(const infra::Function<void(FwuError)>& onResult)
    {
        onResult(FwuError::notReady);
    }

    void FirmwareUpgrade::OnAbort(const infra::Function<void()>& onDone)
    {
        if (flashBusy)
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

    bool FirmwareUpgrade::Holds(State state)
    {
        return state == State::erasing || state == State::receiving || state == State::verifying;
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

        if (Downloading() || flashBusy || !drive.IsStoppedAndIdle())
            return FwuError::busy;

        if (firmwareSize < minimumPackSize || firmwareSize > staging->pack.TotalSize())
            return FwuError::invalidSize;

        return std::nullopt;
    }

    void FirmwareUpgrade::StartDownload(uint32_t firmwareSize)
    {
        packSize = firmwareSize;
        received = 0;
        nextBlock = 0;
        pageAddress = 0;
        pageFill = 0;
        carryFill = 0;
        eraseSector = 0;
        stagedVersion = {};
        Enter(State::erasing);

        if (journal->Status() != application::UpgradePackStatus::empty)
            journal->Clear();

        EraseNextSector();
    }

    void FirmwareUpgrade::EraseNextSector()
    {
        if (eraseSector == staging->pack.NumberOfSectors())
        {
            OnSlotErased();
            return;
        }

        flashBusy = true;
        staging->pack.EraseSectors(eraseSector, eraseSector + 1, [this]()
            {
                OnSectorErased();
            });
    }

    void FirmwareUpgrade::OnSectorErased()
    {
        if (!FlashDone())
            return;

        readAddress = staging->pack.AddressOfSector(eraseSector);
        readEnd = staging->pack.AddressOfSector(eraseSector + 1);
        CheckErasedChunk();
    }

    void FirmwareUpgrade::CheckErasedChunk()
    {
        if (readAddress == readEnd)
        {
            ++eraseSector;
            EraseNextSector();
            return;
        }

        const auto length = std::min<uint32_t>(maxPageSize, readEnd - readAddress);
        flashBusy = true;
        staging->pack.ReadBuffer(infra::Head(infra::MakeRange(readBack), length), readAddress, [this, length]()
            {
                OnErasedChunk(length);
            });
    }

    void FirmwareUpgrade::OnErasedChunk(uint32_t length)
    {
        if (!FlashDone())
            return;

        const auto chunk = infra::Head(infra::MakeRange(readBack), length);
        if (!std::ranges::all_of(chunk, [](uint8_t byte)
                {
                    return byte == 0xff;
                }))
        {
            Fail(FwuError::writeError);
            return;
        }

        readAddress += length;
        CheckErasedChunk();
    }

    void FirmwareUpgrade::OnSlotErased()
    {
        Enter(State::receiving);
        RestartDeadline();
        Take(beginDone)(FwuError::ok, staging->pageSize);
    }

    void FirmwareUpgrade::AcceptBlock(infra::ConstByteRange data)
    {
        received += static_cast<uint32_t>(data.size());
        const auto now = std::min<std::size_t>(staging->pageSize - pageFill, data.size());
        std::copy(data.begin(), data.begin() + now, page.begin() + pageFill);
        pageFill += static_cast<uint16_t>(now);

        const auto rest = infra::Head(infra::DiscardHead(data, now), carry.size());
        std::ranges::copy(rest, carry.begin());
        carryFill = static_cast<uint8_t>(rest.size());

        if (pageFill == staging->pageSize)
            ProgramPage();
        else
            CompleteBlock();
    }

    void FirmwareUpgrade::ProgramPage()
    {
        flashBusy = true;
        staging->pack.WriteBuffer(infra::Head(infra::MakeRange(page), pageFill), pageAddress, [this]()
            {
                OnPageWritten();
            });
    }

    void FirmwareUpgrade::OnPageWritten()
    {
        if (!FlashDone())
            return;

        flashBusy = true;
        staging->pack.ReadBuffer(infra::Head(infra::MakeRange(readBack), pageFill), pageAddress, [this]()
            {
                OnPageProgrammed();
            });
    }

    void FirmwareUpgrade::OnPageProgrammed()
    {
        if (!FlashDone())
            return;

        if (!std::equal(page.begin(), page.begin() + pageFill, readBack.begin()))
        {
            Fail(FwuError::writeError);
            return;
        }

        pageAddress += pageFill;
        std::copy(carry.begin(), carry.begin() + carryFill, page.begin());
        pageFill = carryFill;
        carryFill = 0;

        if (state == State::verifying)
            StartCrc();
        else
            CompleteBlock();
    }

    void FirmwareUpgrade::CompleteBlock()
    {
        Take(blockDone)(FwuError::ok);
    }

    void FirmwareUpgrade::StartCrc()
    {
        crc.Reset();
        readAddress = 0;
        readEnd = packSize;
        ReadNextCrcChunk();
    }

    void FirmwareUpgrade::ReadNextCrcChunk()
    {
        if (readAddress == readEnd)
        {
            if (crc.Result() != expectedCrc)
                Fail(FwuError::crcMismatch);
            else
                ReadHeader([this]()
                    {
                        OnVerifiedHeader();
                    });
            return;
        }

        const auto length = std::min<uint32_t>(maxPageSize, readEnd - readAddress);
        flashBusy = true;
        staging->pack.ReadBuffer(infra::Head(infra::MakeRange(readBack), length), readAddress, [this, length]()
            {
                OnCrcChunk(length);
            });
    }

    void FirmwareUpgrade::OnCrcChunk(uint32_t length)
    {
        if (!FlashDone())
            return;

        crc.Update(infra::Head(infra::MakeRange(readBack), length));
        readAddress += length;
        ReadNextCrcChunk();
    }

    void FirmwareUpgrade::ReadHeader(const infra::Function<void()>& onRead)
    {
        onHeaderRead = onRead;
        header = {};
        flashBusy = true;
        staging->pack.ReadBuffer(infra::MakeByteRange(header.prologue), 0, [this]()
            {
                ReadEpilogue();
            });
    }

    void FirmwareUpgrade::ReadEpilogue()
    {
        if (!FlashDone())
            return;

        const auto offset = EpilogueOffset(header.prologue);
        if (offset + sizeof(header.epilogue) > packSize)
        {
            FinishHeader();
            return;
        }

        flashBusy = true;
        staging->pack.ReadBuffer(infra::MakeByteRange(header.epilogue), offset, [this]()
            {
                ReadImagePrologue();
            });
    }

    void FirmwareUpgrade::ReadImagePrologue()
    {
        if (!FlashDone())
            return;

        const uint32_t offset = header.epilogue.headerLength;
        if (offset + sizeof(header.image) > packSize)
        {
            FinishHeader();
            return;
        }

        flashBusy = true;
        staging->pack.ReadBuffer(infra::MakeByteRange(header.image), offset, [this]()
            {
                if (FlashDone())
                    FinishHeader();
            });
    }

    void FirmwareUpgrade::FinishHeader()
    {
        Take(onHeaderRead)();
    }

    void FirmwareUpgrade::OnVerifiedHeader()
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

    bool FirmwareUpgrade::FlashDone()
    {
        flashBusy = false;
        return !TakeAbort();
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

        if (flashBusy)
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
                return FwuState::complete;
            case State::error:
                return FwuState::error;
            default:
                return FwuState::idle;
        }
    }
}
