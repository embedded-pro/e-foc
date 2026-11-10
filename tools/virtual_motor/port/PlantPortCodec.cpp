#include "tools/virtual_motor/port/PlantPortCodec.hpp"
#include <algorithm>
#include <limits>

namespace virtual_motor::port
{
    namespace
    {
        constexpr std::size_t headerSize{ 3 };
    }

    std::vector<uint8_t> EncodeFrame(uint8_t type, const std::vector<uint8_t>& payload)
    {
        const auto size = std::min<std::size_t>(payload.size(), std::numeric_limits<uint16_t>::max());
        std::vector<uint8_t> frame{ type, static_cast<uint8_t>(size), static_cast<uint8_t>(size >> 8) };
        frame.insert(frame.end(), payload.begin(), payload.begin() + static_cast<std::ptrdiff_t>(size));
        return frame;
    }

    void FrameAssembler::Append(const std::vector<uint8_t>& bytes)
    {
        pending.insert(pending.end(), bytes.begin(), bytes.end());
    }

    void FrameAssembler::Append(const uint8_t* data, std::size_t size)
    {
        pending.insert(pending.end(), data, data + size);
    }

    std::optional<Frame> FrameAssembler::Next()
    {
        if (pending.size() < headerSize)
            return std::nullopt;

        const std::size_t size = pending[1] | (static_cast<std::size_t>(pending[2]) << 8);
        if (pending.size() < headerSize + size)
            return std::nullopt;

        Frame frame{ pending[0], { pending.begin() + headerSize, pending.begin() + static_cast<std::ptrdiff_t>(headerSize + size) } };
        pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(headerSize + size));
        return frame;
    }
}
