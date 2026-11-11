#pragma once

#include "tools/virtual_motor/port/PlantPortMessages.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace virtual_motor::port
{
    struct Frame
    {
        uint8_t type{ 0 };
        std::vector<uint8_t> payload;
    };

    std::vector<uint8_t> EncodeFrame(uint8_t type, const std::vector<uint8_t>& payload);

    template<class Message>
    std::vector<uint8_t> Encode(const Message& message)
    {
        PayloadWriter writer;
        message.WriteTo(writer);
        return EncodeFrame(static_cast<uint8_t>(Message::type), writer.Bytes());
    }

    template<class Message>
    std::optional<Message> Decode(const Frame& frame)
    {
        if (frame.type != static_cast<uint8_t>(Message::type))
            return std::nullopt;

        PayloadReader reader{ frame.payload };
        auto message = Message::ReadFrom(reader);
        if (!reader.Valid())
            return std::nullopt;
        return message;
    }

    class FrameAssembler
    {
    public:
        void Append(const std::vector<uint8_t>& bytes);
        void Append(const uint8_t* data, std::size_t size);
        std::optional<Frame> Next();

    private:
        std::vector<uint8_t> pending;
    };
}
