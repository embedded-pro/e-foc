#include "tools/virtual_motor/port/PlantPortPayload.hpp"
#include <algorithm>
#include <bit>
#include <limits>

namespace virtual_motor::port
{
    void PayloadWriter::U8(uint8_t value)
    {
        bytes.push_back(value);
    }

    void PayloadWriter::U16(uint16_t value)
    {
        Little(value, sizeof(value));
    }

    void PayloadWriter::U32(uint32_t value)
    {
        Little(value, sizeof(value));
    }

    void PayloadWriter::U64(uint64_t value)
    {
        Little(value, sizeof(value));
    }

    void PayloadWriter::F32(float value)
    {
        U32(std::bit_cast<uint32_t>(value));
    }

    void PayloadWriter::Text(std::string_view text)
    {
        const auto size = std::min<std::size_t>(text.size(), std::numeric_limits<uint8_t>::max());
        U8(static_cast<uint8_t>(size));
        bytes.insert(bytes.end(), text.begin(), text.begin() + static_cast<std::ptrdiff_t>(size));
    }

    const std::vector<uint8_t>& PayloadWriter::Bytes() const
    {
        return bytes;
    }

    void PayloadWriter::Little(uint64_t value, std::size_t size)
    {
        for (std::size_t index = 0; index != size; ++index)
            bytes.push_back(static_cast<uint8_t>(value >> (8 * index)));
    }

    PayloadReader::PayloadReader(const std::vector<uint8_t>& bytes)
        : bytes(bytes)
    {}

    uint8_t PayloadReader::U8()
    {
        return static_cast<uint8_t>(Little(sizeof(uint8_t)));
    }

    uint16_t PayloadReader::U16()
    {
        return static_cast<uint16_t>(Little(sizeof(uint16_t)));
    }

    uint32_t PayloadReader::U32()
    {
        return static_cast<uint32_t>(Little(sizeof(uint32_t)));
    }

    uint64_t PayloadReader::U64()
    {
        return Little(sizeof(uint64_t));
    }

    float PayloadReader::F32()
    {
        return std::bit_cast<float>(U32());
    }

    std::string PayloadReader::Text()
    {
        const std::size_t size{ U8() };
        if (!valid || bytes.size() - offset < size)
        {
            valid = false;
            return {};
        }

        std::string text(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
        offset += size;
        return text;
    }

    bool PayloadReader::Valid() const
    {
        return valid;
    }

    uint64_t PayloadReader::Little(std::size_t size)
    {
        if (!valid || bytes.size() - offset < size)
        {
            valid = false;
            return 0;
        }

        uint64_t value{ 0 };
        for (std::size_t index = 0; index != size; ++index)
            value |= static_cast<uint64_t>(bytes[offset + index]) << (8 * index);
        offset += size;
        return value;
    }
}
