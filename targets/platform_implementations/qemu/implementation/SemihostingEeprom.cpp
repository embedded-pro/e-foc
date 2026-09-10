#include "targets/platform_implementations/qemu/implementation/SemihostingEeprom.hpp"
#include "hal/cortex_m/Semihosting.hpp"
#include <cstdint>
#include <cstring>

namespace
{
    constexpr uint32_t kSysOpen = 0x01;
    constexpr uint32_t kSysClose = 0x02;
    constexpr uint32_t kSysWrite = 0x05;
    constexpr uint32_t kSysRead = 0x06;

    constexpr uint32_t kOpenModeRb = 1;
    constexpr uint32_t kOpenModeWb = 5;

    int FileOpen(const char* path, uint32_t mode)
    {
        uint32_t params[3] = {
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(path)),
            mode,
            static_cast<uint32_t>(std::strlen(path))
        };
        return static_cast<int>(hal::cortex::SemihostingCall(
            static_cast<hal::cortex::SemihostingOperation>(kSysOpen), params));
    }

    void FileClose(int handle)
    {
        uint32_t param = static_cast<uint32_t>(handle);
        hal::cortex::SemihostingCall(
            static_cast<hal::cortex::SemihostingOperation>(kSysClose), &param);
    }

    // Returns 0 on success (all bytes written), positive = bytes not written.
    uint32_t FileWrite(int handle, const void* data, uint32_t len)
    {
        uint32_t params[3] = {
            static_cast<uint32_t>(handle),
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data)),
            len
        };
        return hal::cortex::SemihostingCall(
            static_cast<hal::cortex::SemihostingOperation>(kSysWrite), params);
    }

    // Returns 0 on success (all bytes read), positive = bytes not read.
    uint32_t FileRead(int handle, void* buf, uint32_t len)
    {
        uint32_t params[3] = {
            static_cast<uint32_t>(handle),
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buf)),
            len
        };
        return hal::cortex::SemihostingCall(
            static_cast<hal::cortex::SemihostingOperation>(kSysRead), params);
    }
}

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
        const int handle = FileOpen(filePath, kOpenModeRb);
        if (handle < 0)
            return;

        FileRead(handle, storage.data(), storageSize);
        FileClose(handle);
    }

    void SemihostingEeprom::FlushToFile() const
    {
        const int handle = FileOpen(filePath, kOpenModeWb);
        if (handle < 0)
            return;
        FileWrite(handle, storage.data(), storageSize);
        FileClose(handle);
    }
}
