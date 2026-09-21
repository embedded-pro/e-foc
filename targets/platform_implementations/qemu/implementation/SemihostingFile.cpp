#include "targets/platform_implementations/qemu/implementation/SemihostingFile.hpp"
#include "hal/cortex_m/Semihosting.hpp"
#include <cstring>

namespace
{
    constexpr uint32_t kSysOpen = 0x01;
    constexpr uint32_t kSysClose = 0x02;
    constexpr uint32_t kSysWrite = 0x05;
    constexpr uint32_t kSysRead = 0x06;

    uint32_t Call(uint32_t operation, uint32_t* parameters)
    {
        return hal::cortex::SemihostingCall(
            static_cast<hal::cortex::SemihostingOperation>(operation), parameters);
    }
}

namespace application::semihosting
{
    int FileOpen(const char* path, OpenMode mode)
    {
        uint32_t parameters[3] = {
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(path)),
            static_cast<uint32_t>(mode),
            static_cast<uint32_t>(std::strlen(path))
        };
        return static_cast<int>(Call(kSysOpen, parameters));
    }

    void FileClose(int handle)
    {
        uint32_t parameter = static_cast<uint32_t>(handle);
        Call(kSysClose, &parameter);
    }

    uint32_t FileWrite(int handle, const void* data, uint32_t length)
    {
        uint32_t parameters[3] = {
            static_cast<uint32_t>(handle),
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data)),
            length
        };
        return Call(kSysWrite, parameters);
    }

    uint32_t FileRead(int handle, void* buffer, uint32_t length)
    {
        uint32_t parameters[3] = {
            static_cast<uint32_t>(handle),
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buffer)),
            length
        };
        return Call(kSysRead, parameters);
    }
}
