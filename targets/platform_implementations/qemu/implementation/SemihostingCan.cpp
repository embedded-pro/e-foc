#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "targets/platform_implementations/qemu/implementation/SemihostingCan.hpp"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace
{
    constexpr int hexDigitValue(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }

    bool TryReadLine(char* buf, int size)
    {
        return std::fgets(buf, size, stdin) != nullptr;
    }

    bool ParseHexByte(const char* s, uint8_t& out)
    {
        const int hi = hexDigitValue(s[0]);
        const int lo = hexDigitValue(s[1]);
        if (hi < 0 || lo < 0)
            return false;
        out = static_cast<uint8_t>((hi << 4) | lo);
        return true;
    }

    bool ParseCanLine(const char* line, hal::Can::Id& id, hal::Can::Message& msg)
    {
        constexpr const char* prefix = "CAN_RX ";
        constexpr int prefixLen = 7;
        if (std::strncmp(line, prefix, static_cast<std::size_t>(prefixLen)) != 0)
            return false;

        const char* idStr = line + prefixLen;
        const char* space = std::strchr(idStr, ' ');
        if (space == nullptr)
            return false;

        const int idLen = static_cast<int>(space - idStr);
        if (idLen < 1 || idLen > 8)
            return false;

        uint32_t rawId = 0;
        for (int i = 0; i < idLen; ++i)
        {
            const int d = hexDigitValue(idStr[i]);
            if (d < 0)
                return false;
            rawId = (rawId << 4) | static_cast<uint32_t>(d);
        }

        const char* dataStr = space + 1;
        const int dataLen = static_cast<int>(std::strlen(dataStr));

        for (int i = 0; i + 1 < dataLen; i += 2)
        {
            const char ch = dataStr[i];
            if (ch == '\n' || ch == '\r' || ch == '\0')
                break;
            uint8_t byte{};
            if (!ParseHexByte(dataStr + i, byte))
                break;
            if (msg.full())
                break;
            msg.push_back(byte);
        }

        id = hal::Can::Id::Create29BitId(rawId);
        return true;
    }
}

namespace sil
{
    SemihostingCan::SemihostingCan()
    {
        const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (flags >= 0 && fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == 0)
            nonBlockingSet = true;
    }

    void SemihostingCan::SendData(Id id, const Message& data, const infra::Function<void(bool)>& onDone)
    {
        char hexData[17]{};
        int pos = 0;
        for (const uint8_t byte : data)
        {
            hexData[pos++] = "0123456789abcdef"[(byte >> 4) & 0xF];
            hexData[pos++] = "0123456789abcdef"[byte & 0xF];
        }
        hexData[pos] = '\0';

        const uint32_t rawId = id.Is11BitId() ? id.Get11BitId() : id.Get29BitId();
        std::printf("CAN_TX %03lx %s\n", static_cast<unsigned long>(rawId), hexData);
        std::fflush(stdout);

        if (onDone)
            onDone(true);
    }

    void SemihostingCan::ReceiveData(const infra::Function<void(Id, const Message&)>& onReceived)
    {
        receiveCallback = onReceived;
    }

    void SemihostingCan::PollIncoming()
    {
        char line[64]{};
        if (!TryReadLine(line, static_cast<int>(sizeof(line))))
            return;

        hal::Can::Id id{ hal::Can::Id::Create11BitId(0) };
        hal::Can::Message msg;
        if (!ParseCanLine(line, id, msg))
            return;

        if (receiveCallback)
            receiveCallback(id, msg);
    }
}
