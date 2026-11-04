#include "targets/platform_implementations/qemu/implementation/SemihostingFileFlash.hpp"
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
        Load();
    }

    void SemihostingFileFlash::WriteBuffer(infra::ConstByteRange buffer, uint32_t address)
    {
        services::SynchronousFlashMemory::WriteBuffer(buffer, address);
        Store();
    }

    void SemihostingFileFlash::EraseSectors(uint32_t beginIndex, uint32_t endIndex)
    {
        services::SynchronousFlashMemory::EraseSectors(beginIndex, endIndex);
        Store();
    }

    void SemihostingFileFlash::Load()
    {
        const int handle = semihosting::FileOpen(path, semihosting::OpenMode::readBinary);
        if (handle < 0)
            return;

        semihosting::FileRead(handle, storage.begin(), static_cast<uint32_t>(storage.size()));
        semihosting::FileClose(handle);
    }

    void SemihostingFileFlash::Store() const
    {
        const int handle = semihosting::FileOpen(path, semihosting::OpenMode::writeBinary);
        if (handle < 0)
            return;

        semihosting::FileWrite(handle, storage.begin(), static_cast<uint32_t>(storage.size()));
        semihosting::FileClose(handle);
    }
}
