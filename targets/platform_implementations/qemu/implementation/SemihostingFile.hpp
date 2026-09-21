#pragma once

#include <cstdint>

namespace application::semihosting
{
    enum class OpenMode : uint32_t
    {
        readBinary = 1,
        writeBinary = 5
    };

    int FileOpen(const char* path, OpenMode mode);
    void FileClose(int handle);

    uint32_t FileWrite(int handle, const void* data, uint32_t length);
    uint32_t FileRead(int handle, void* buffer, uint32_t length);
}
