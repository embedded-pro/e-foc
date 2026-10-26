#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace application
{
    class SemihostingRecordFile
    {
    public:
        explicit SemihostingRecordFile(const char* path);

        void Append(std::string_view text);
        void Flush();

    private:
        int handle;
        std::array<char, 1024> buffer{};
        std::size_t used{ 0 };
    };
}
