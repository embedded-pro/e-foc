#include "targets/platform_implementations/qemu/implementation/SemihostingRecordFile.hpp"
#include "targets/platform_implementations/qemu/implementation/SemihostingFile.hpp"
#include <algorithm>
#include <cstdint>

namespace application
{
    SemihostingRecordFile::SemihostingRecordFile(const char* path)
        : handle(semihosting::FileOpen(path, semihosting::OpenMode::writeBinary))
    {}

    void SemihostingRecordFile::Append(std::string_view text)
    {
        if (handle < 0)
            return;

        if (text.size() > buffer.size() - used)
            Flush();

        const auto length = std::min(text.size(), buffer.size());
        std::copy_n(text.begin(), length, buffer.begin() + static_cast<std::ptrdiff_t>(used));
        used += length;
    }

    void SemihostingRecordFile::Flush()
    {
        if (handle < 0 || used == 0)
            return;

        semihosting::FileWrite(handle, buffer.data(), static_cast<uint32_t>(used));
        used = 0;
    }
}
