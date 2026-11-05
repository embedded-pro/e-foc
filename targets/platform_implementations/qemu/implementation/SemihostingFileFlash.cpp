#include "targets/platform_implementations/qemu/implementation/SemihostingFileFlash.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingFile.hpp"
#include <algorithm>

namespace application
{
    SemihostingFileFlash::SemihostingFileFlash(const char* path, infra::ByteRange storage, uint32_t sizeOfEachSector)
        : services::SynchronousFlashMemory(storage, sizeOfEachSector)
        , path(path)
        , storage(storage)
    {
        std::fill(storage.begin(), storage.end(), 0xff);
        if (!Load())
            StoreAll();
    }

    void SemihostingFileFlash::WriteBuffer(infra::ConstByteRange buffer, uint32_t address)
    {
        services::SynchronousFlashMemory::WriteBuffer(buffer, address);
        Store(address, static_cast<uint32_t>(buffer.size()));
    }

    void SemihostingFileFlash::EraseSectors(uint32_t beginIndex, uint32_t endIndex)
    {
        services::SynchronousFlashMemory::EraseSectors(beginIndex, endIndex);
        const auto begin = AddressOfSector(beginIndex);
        Store(begin, AddressOfSector(endIndex) - begin);
    }

    bool SemihostingFileFlash::Load()
    {
        const int handle = semihosting::FileOpen(path, semihosting::OpenMode::readBinary);
        if (handle < 0)
            return false;

        semihosting::FileRead(handle, storage.begin(), static_cast<uint32_t>(storage.size()));
        semihosting::FileClose(handle);
        return true;
    }

    void SemihostingFileFlash::StoreAll() const
    {
        const int handle = semihosting::FileOpen(path, semihosting::OpenMode::writeBinary);
        really_assert(handle >= 0);
        really_assert(semihosting::FileWrite(handle, storage.begin(), static_cast<uint32_t>(storage.size())) == 0);
        semihosting::FileClose(handle);
    }

    void SemihostingFileFlash::Store(uint32_t address, uint32_t length) const
    {
        if (length == 0)
            return;

        const int handle = semihosting::FileOpen(path, semihosting::OpenMode::readWriteBinary);
        really_assert(handle >= 0);
        really_assert(semihosting::FileSeek(handle, address));
        really_assert(semihosting::FileWrite(handle, storage.begin() + address, length) == 0);
        semihosting::FileClose(handle);
    }
}
