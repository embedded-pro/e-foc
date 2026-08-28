#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "integration_tests/software_in_the_loop/qemu_target/SemihostingCan.hpp"
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
}

namespace sil
{
    SemihostingCan::SemihostingCan()
    {
        const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (flags >= 0)
            fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
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
        if (std::fgets(line, static_cast<int>(sizeof(line)), stdin) == nullptr)
            return;

        const char* prefix = "CAN_RX ";
        const int prefixLen = 7;
        if (std::strncmp(line, prefix, static_cast<std::size_t>(prefixLen)) != 0)
            return;

        const char* idStr = line + prefixLen;
        const char* space = std::strchr(idStr, ' ');
        if (space == nullptr)
            return;

        const int idLen = static_cast<int>(space - idStr);
        if (idLen < 1 || idLen > 8)
            return;

        uint32_t rawId = 0;
        for (int i = 0; i < idLen; ++i)
        {
            const int d = hexDigitValue(idStr[i]);
            if (d < 0)
                return;
            rawId = (rawId << 4) | static_cast<uint32_t>(d);
        }

        const char* dataStr = space + 1;
        hal::Can::Message msg;
        const int dataLen = static_cast<int>(std::strlen(dataStr));

        for (int i = 0; i + 1 < dataLen; i += 2)
        {
            const char ch = dataStr[i];
            if (ch == '\n' || ch == '\r' || ch == '\0')
                break;
            const int hi = hexDigitValue(ch);
            const int lo = hexDigitValue(dataStr[i + 1]);
            if (hi < 0 || lo < 0)
                break;
            if (msg.full())
                break;
            msg.push_back(static_cast<uint8_t>((hi << 4) | lo));
        }

        const hal::Can::Id canId = hal::Can::Id::Create11BitId(rawId);

        if (receiveCallback)
            receiveCallback(canId, msg);
    }
}
