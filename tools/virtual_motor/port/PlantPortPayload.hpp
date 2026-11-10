#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace virtual_motor::port
{
    class PayloadWriter
    {
    public:
        void U8(uint8_t value);
        void U16(uint16_t value);
        void U32(uint32_t value);
        void U64(uint64_t value);
        void F32(float value);
        void Text(std::string_view text);

        const std::vector<uint8_t>& Bytes() const;

    private:
        void Little(uint64_t value, std::size_t size);

        std::vector<uint8_t> bytes;
    };

    class PayloadReader
    {
    public:
        explicit PayloadReader(const std::vector<uint8_t>& bytes);

        uint8_t U8();
        uint16_t U16();
        uint32_t U32();
        uint64_t U64();
        float F32();
        std::string Text();

        bool Valid() const;

    private:
        uint64_t Little(std::size_t size);

        const std::vector<uint8_t>& bytes;
        std::size_t offset{ 0 };
        bool valid{ true };
    };
}
