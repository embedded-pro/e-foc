#include "core/services/firmware_upgrade/StagingWriter.hpp"
#include <algorithm>

namespace services
{
    StagingWriter::StagingWriter(hal::Flash& flash, uint16_t pageSize, infra::ByteRange scratch, const infra::Function<bool()>& proceed)
        : flash(flash)
        , pageSize(pageSize)
        , scratch(scratch)
        , proceed(proceed)
    {}

    void StagingWriter::Erase(const infra::Function<void(bool)>& onErased)
    {
        onDone = onErased;
        written = 0;
        pageAddress = 0;
        pageFill = 0;
        carryFill = 0;
        eraseSector = 0;
        EraseNextSector();
    }

    void StagingWriter::Append(infra::ConstByteRange data, const infra::Function<void(bool)>& onAppended)
    {
        onDone = onAppended;
        written += static_cast<uint32_t>(data.size());
        const auto now = std::min<std::size_t>(pageSize - pageFill, data.size());
        std::copy(data.begin(), data.begin() + now, page.begin() + pageFill);
        pageFill += static_cast<uint16_t>(now);

        const auto rest = infra::Head(infra::DiscardHead(data, now), carry.size());
        std::ranges::copy(rest, carry.begin());
        carryFill = static_cast<uint8_t>(rest.size());

        if (pageFill == pageSize)
            ProgramPage();
        else
            Finish(true);
    }

    void StagingWriter::Flush(const infra::Function<void(bool)>& onFlushed)
    {
        onDone = onFlushed;
        ProgramPage();
    }

    bool StagingWriter::Busy() const
    {
        return busy;
    }

    uint32_t StagingWriter::Written() const
    {
        return written;
    }

    bool StagingWriter::HoldsPartialPage() const
    {
        return pageFill != 0;
    }

    void StagingWriter::EraseNextSector()
    {
        if (eraseSector == flash.NumberOfSectors())
        {
            Finish(true);
            return;
        }

        busy = true;
        flash.EraseSectors(eraseSector, eraseSector + 1, [this]()
            {
                OnSectorErased();
            });
    }

    void StagingWriter::OnSectorErased()
    {
        if (!StepDone())
            return;

        readAddress = flash.AddressOfSector(eraseSector);
        readEnd = flash.AddressOfSector(eraseSector + 1);
        CheckErasedChunk();
    }

    void StagingWriter::CheckErasedChunk()
    {
        if (readAddress == readEnd)
        {
            ++eraseSector;
            EraseNextSector();
            return;
        }

        const auto length = std::min<uint32_t>(static_cast<uint32_t>(scratch.size()), readEnd - readAddress);
        busy = true;
        flash.ReadBuffer(infra::Head(scratch, length), readAddress, [this, length]()
            {
                OnErasedChunk(length);
            });
    }

    void StagingWriter::OnErasedChunk(uint32_t length)
    {
        if (!StepDone())
            return;

        if (!std::ranges::all_of(infra::Head(scratch, length), [](uint8_t byte)
                {
                    return byte == 0xff;
                }))
        {
            Finish(false);
            return;
        }

        readAddress += length;
        CheckErasedChunk();
    }

    void StagingWriter::ProgramPage()
    {
        busy = true;
        flash.WriteBuffer(infra::Head(infra::MakeRange(page), pageFill), pageAddress, [this]()
            {
                OnPageWritten();
            });
    }

    void StagingWriter::OnPageWritten()
    {
        if (!StepDone())
            return;

        busy = true;
        flash.ReadBuffer(infra::Head(scratch, pageFill), pageAddress, [this]()
            {
                OnPageProgrammed();
            });
    }

    void StagingWriter::OnPageProgrammed()
    {
        if (!StepDone())
            return;

        if (!std::equal(page.begin(), page.begin() + pageFill, scratch.begin()))
        {
            Finish(false);
            return;
        }

        pageAddress += pageFill;
        std::copy(carry.begin(), carry.begin() + carryFill, page.begin());
        pageFill = carryFill;
        carryFill = 0;
        Finish(true);
    }

    bool StagingWriter::StepDone()
    {
        busy = false;
        if (proceed())
            return true;

        onDone = nullptr;
        return false;
    }

    void StagingWriter::Finish(bool ok)
    {
        auto done = onDone;
        onDone = nullptr;
        done(ok);
    }
}
