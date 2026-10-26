#include "targets/platform_implementations/qemu/implementation/EmulatedCan.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr const char* receivedPrefix = "CAN_RX ";
    constexpr std::size_t receivedPrefixLength = 7;

    constexpr int HexDigitValue(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }

    bool ParseHexByte(const char* s, uint8_t& out)
    {
        const int hi = HexDigitValue(s[0]);
        const int lo = HexDigitValue(s[1]);
        if (hi < 0 || lo < 0)
            return false;
        out = static_cast<uint8_t>((hi << 4) | lo);
        return true;
    }

    bool ParseCanLine(const char* line, hal::Can::Id& id, hal::Can::Message& msg)
    {
        if (std::strncmp(line, receivedPrefix, receivedPrefixLength) != 0)
            return false;

        const char* idStr = line + receivedPrefixLength;
        const char* space = std::strchr(idStr, ' ');
        if (space == nullptr)
            return false;

        const auto idLen = space - idStr;
        if (idLen < 1 || idLen > 8)
            return false;

        uint32_t rawId = 0;
        for (const char* digit = idStr; digit != space; ++digit)
        {
            const int value = HexDigitValue(*digit);
            if (value < 0)
                return false;
            rawId = (rawId << 4) | static_cast<uint32_t>(value);
        }

        for (const char* pair = space + 1; pair[0] != '\0' && pair[1] != '\0' && !msg.full(); pair += 2)
        {
            uint8_t byte{};
            if (!ParseHexByte(pair, byte))
                break;
            msg.push_back(byte);
        }

        id = hal::Can::Id::Create29BitId(rawId);
        return true;
    }
}

namespace sil
{
    EmulatedCan::EmulatedCan(uintptr_t uartBase)
        : port(uartBase)
    {}

    void EmulatedCan::SendData(Id id, const Message& data, const infra::Function<void(bool)>& onDone)
    {
        char hexData[17]{};
        int pos = 0;
        for (const uint8_t byte : data)
        {
            hexData[pos++] = "0123456789abcdef"[(byte >> 4) & 0xF];
            hexData[pos++] = "0123456789abcdef"[byte & 0xF];
        }

        const uint32_t rawId = id.Is11BitId() ? id.Get11BitId() : id.Get29BitId();
        char line[40]{};
        const int length = std::snprintf(line, sizeof(line), "CAN_TX %lx %s\n", static_cast<unsigned long>(rawId), hexData);
        if (length > 0)
            port.Write(std::string_view{ line, static_cast<std::size_t>(length) });

        if (onDone)
            onDone(true);
    }

    void EmulatedCan::ReceiveData(const infra::Function<void(Id, const Message&)>& onReceived)
    {
        receiveCallback = onReceived;
    }

    std::optional<EmulatedCan::Frame> EmulatedCan::PollIncoming()
    {
        const char* line = port.PollLine();
        if (line == nullptr)
            return std::nullopt;

        hal::Can::Id id{ hal::Can::Id::Create11BitId(0) };
        hal::Can::Message msg;
        if (!ParseCanLine(line, id, msg))
            return std::nullopt;

        if (receiveCallback)
            receiveCallback(id, msg);

        return Frame{ id, msg };
    }
}
