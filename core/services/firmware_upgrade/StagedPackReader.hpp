#pragma once

#include "core/services/firmware_upgrade/PackHeaderCheck.hpp"
#include "hal/interfaces/Flash.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Crc.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace services
{
    class StagedPackReader
    {
    public:
        StagedPackReader(hal::Flash& flash, infra::ByteRange scratch, const infra::Function<bool()>& proceed);
        StagedPackReader(const StagedPackReader&) = delete;
        StagedPackReader& operator=(const StagedPackReader&) = delete;
        StagedPackReader(StagedPackReader&&) = delete;
        StagedPackReader& operator=(StagedPackReader&&) = delete;

        void CheckCrc(uint32_t size, uint32_t expected, const infra::Function<void(bool)>& onChecked);
        void ReadHeader(uint32_t limit, const infra::Function<void(const PackHeader&)>& onRead);

        bool Busy() const;

    private:
        void ReadNextChunk();
        void OnChunk(uint32_t length);
        void ReadEpilogue();
        void ReadImagePrologue();
        void FinishHeader();
        bool StepDone();

        hal::Flash& flash;
        infra::ByteRange scratch;
        infra::Function<bool()> proceed;
        bool busy{ false };

        uint32_t readAddress{ 0 };
        uint32_t readEnd{ 0 };
        uint32_t expectedCrc{ 0 };
        infra::Crc32 crc;
        infra::Function<void(bool)> onCrcChecked;

        uint32_t headerLimit{ 0 };
        PackHeader header{};
        infra::Function<void(const PackHeader&)> onHeaderRead;
    };
}
