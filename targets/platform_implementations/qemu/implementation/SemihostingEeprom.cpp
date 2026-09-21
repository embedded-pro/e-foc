#include "targets/platform_implementations/qemu/implementation/SemihostingEeprom.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingFile.hpp"
#include <cstdint>

namespace application
{
    SemihostingEeprom::SemihostingEeprom(const char* path)
        : filePath{ path }
    {
        storage.fill(0xFF);
        LoadFromFile();
    }

    uint32_t SemihostingEeprom::Size() const
    {
        return storageSize;
    }

    void SemihostingEeprom::WriteBuffer(infra::ConstByteRange buffer, uint32_t address,
        infra::Function<void()> onDone)
    {
        for (std::size_t i = 0; i < buffer.size() && (address + i) < storageSize; ++i)
            storage[address + i] = buffer[i];
        FlushToFile();
        if (onDone)
            onDone();
    }

    void SemihostingEeprom::ReadBuffer(infra::ByteRange buffer, uint32_t address,
        infra::Function<void()> onDone)
    {
        for (std::size_t i = 0; i < buffer.size() && (address + i) < storageSize; ++i)
            buffer[i] = storage[address + i];
        if (onDone)
            onDone();
    }

    void SemihostingEeprom::Erase(infra::Function<void()> onDone)
    {
        storage.fill(0xFF);
        FlushToFile();
        if (onDone)
            onDone();
    }

    void SemihostingEeprom::LoadFromFile()
    {
        const int handle = semihosting::FileOpen(filePath, semihosting::OpenMode::readBinary);
        if (handle < 0)
            return;

        semihosting::FileRead(handle, storage.data(), storageSize);
        semihosting::FileClose(handle);
    }

    void SemihostingEeprom::FlushToFile() const
    {
        const int handle = semihosting::FileOpen(filePath, semihosting::OpenMode::writeBinary);
        if (handle < 0)
            return;
        semihosting::FileWrite(handle, storage.data(), storageSize);
        semihosting::FileClose(handle);
    }
}
