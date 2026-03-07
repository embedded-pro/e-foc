#include "source/services/can_protocol/core/CanFrameCodec.hpp"
#include <cmath>

namespace services
{
    int16_t CanFrameCodec::FloatToFixed16(float value, int32_t scale)
    {
        float scaled = std::roundf(value * static_cast<float>(scale));
        if (scaled > static_cast<float>(std::numeric_limits<int16_t>::max()))
            return std::numeric_limits<int16_t>::max();
        if (scaled < static_cast<float>(std::numeric_limits<int16_t>::min()))
            return std::numeric_limits<int16_t>::min();
        return static_cast<int16_t>(scaled);
    }

    float CanFrameCodec::Fixed16ToFloat(int16_t value, int32_t scale)
    {
        return static_cast<float>(value) / static_cast<float>(scale);
    }

    int32_t CanFrameCodec::FloatToFixed32(float value, int32_t scale)
    {
        float scaled = std::roundf(value * static_cast<float>(scale));
        if (scaled > static_cast<float>(std::numeric_limits<int32_t>::max()))
            return std::numeric_limits<int32_t>::max();
        if (scaled < static_cast<float>(std::numeric_limits<int32_t>::min()))
            return std::numeric_limits<int32_t>::min();
        return static_cast<int32_t>(scaled);
    }

    float CanFrameCodec::Fixed32ToFloat(int32_t value, int32_t scale)
    {
        return static_cast<float>(value) / static_cast<float>(scale);
    }

    void CanFrameCodec::WriteInt16(hal::Can::Message& msg, std::size_t offset, int16_t value)
    {
        while (msg.size() < offset + 2)
            msg.push_back(0);

        msg[offset] = static_cast<uint8_t>(value >> 8);
        msg[offset + 1] = static_cast<uint8_t>(value & 0xFF);
    }

    int16_t CanFrameCodec::ReadInt16(const hal::Can::Message& msg, std::size_t offset)
    {
        return static_cast<int16_t>(
            (static_cast<uint16_t>(msg[offset]) << 8) |
            static_cast<uint16_t>(msg[offset + 1]));
    }

    void CanFrameCodec::WriteInt32(hal::Can::Message& msg, std::size_t offset, int32_t value)
    {
        while (msg.size() < offset + 4)
            msg.push_back(0);

        msg[offset] = static_cast<uint8_t>((value >> 24) & 0xFF);
        msg[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
        msg[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
        msg[offset + 3] = static_cast<uint8_t>(value & 0xFF);
    }

    int32_t CanFrameCodec::ReadInt32(const hal::Can::Message& msg, std::size_t offset)
    {
        return static_cast<int32_t>(
            (static_cast<uint32_t>(msg[offset]) << 24) |
            (static_cast<uint32_t>(msg[offset + 1]) << 16) |
            (static_cast<uint32_t>(msg[offset + 2]) << 8) |
            static_cast<uint32_t>(msg[offset + 3]));
    }
}
