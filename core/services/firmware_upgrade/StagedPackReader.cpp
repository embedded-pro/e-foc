#include "core/services/firmware_upgrade/StagedPackReader.hpp"
#include <algorithm>

namespace services
{
    StagedPackReader::StagedPackReader(hal::Flash& flash, infra::ByteRange scratch, const infra::Function<bool()>& proceed)
        : flash(flash)
        , scratch(scratch)
        , proceed(proceed)
    {}

    void StagedPackReader::CheckCrc(uint32_t size, uint32_t expected, const infra::Function<void(bool)>& onChecked)
    {
        onCrcChecked = onChecked;
        expectedCrc = expected;
        crc.Reset();
        readAddress = 0;
        readEnd = size;
        ReadNextChunk();
    }

    void StagedPackReader::ReadHeader(uint32_t limit, const infra::Function<void(const PackHeader&)>& onRead)
    {
        onHeaderRead = onRead;
        headerLimit = limit;
        header = {};
        busy = true;
        flash.ReadBuffer(infra::MakeByteRange(header.prologue), 0, [this]()
            {
                ReadEpilogue();
            });
    }

    bool StagedPackReader::Busy() const
    {
        return busy;
    }

    void StagedPackReader::ReadNextChunk()
    {
        if (readAddress == readEnd)
        {
            auto done = onCrcChecked;
            onCrcChecked = nullptr;
            done(crc.Result() == expectedCrc);
            return;
        }

        const auto length = std::min<uint32_t>(static_cast<uint32_t>(scratch.size()), readEnd - readAddress);
        busy = true;
        flash.ReadBuffer(infra::Head(scratch, length), readAddress, [this, length]()
            {
                OnChunk(length);
            });
    }

    void StagedPackReader::OnChunk(uint32_t length)
    {
        if (!StepDone())
            return;

        crc.Update(infra::Head(scratch, length));
        readAddress += length;
        ReadNextChunk();
    }

    void StagedPackReader::ReadEpilogue()
    {
        if (!StepDone())
            return;

        const auto offset = EpilogueOffset(header.prologue);
        if (offset + sizeof(header.epilogue) > headerLimit)
        {
            FinishHeader();
            return;
        }

        busy = true;
        flash.ReadBuffer(infra::MakeByteRange(header.epilogue), offset, [this]()
            {
                ReadImagePrologue();
            });
    }

    void StagedPackReader::ReadImagePrologue()
    {
        if (!StepDone())
            return;

        const uint32_t offset = header.epilogue.headerLength;
        if (offset + sizeof(header.image) > headerLimit)
        {
            FinishHeader();
            return;
        }

        busy = true;
        flash.ReadBuffer(infra::MakeByteRange(header.image), offset, [this]()
            {
                if (StepDone())
                    FinishHeader();
            });
    }

    void StagedPackReader::FinishHeader()
    {
        auto done = onHeaderRead;
        onHeaderRead = nullptr;
        done(header);
    }

    bool StagedPackReader::StepDone()
    {
        busy = false;
        if (proceed())
            return true;

        onCrcChecked = nullptr;
        onHeaderRead = nullptr;
        return false;
    }
}
