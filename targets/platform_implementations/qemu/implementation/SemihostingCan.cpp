#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "targets/platform_implementations/qemu/implementation/SemihostingCan.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    // CMSDK APB UART0 on MPS2-AN386 at 0x40004000.
    // Registers: DATA (0x00), STATE (0x04: bit1=RX_FULL), CTRL (0x08: bit1=RX_EN)
    struct CmsdkUartRegs
    {
        volatile uint32_t data;
        volatile uint32_t state;
        volatile uint32_t ctrl;
        volatile uint32_t intStatus;
        volatile uint32_t baudDiv;
    };

    constexpr uint32_t rxFull = 1u << 1;
    constexpr uint32_t rxEn = 1u << 1;
    constexpr uint32_t txEn = 1u << 0;
    constexpr uintptr_t uart0Base = 0x40004000u;

    CmsdkUartRegs& Uart0()
    {
        return *reinterpret_cast<CmsdkUartRegs*>(uart0Base);
    }

    constexpr std::size_t lineBufSize = 64;
    char lineBuf[lineBufSize]{};
    std::size_t lineBufPos = 0;

    bool DrainUartIntoLine()
    {
        while ((Uart0().state & rxFull) != 0)
        {
            const char c = static_cast<char>(Uart0().data & 0xFFu);
            if (c == '\n' || c == '\r')
            {
                if (lineBufPos > 0)
                {
                    lineBuf[lineBufPos] = '\0';
                    lineBufPos = 0;
                    return true;
                }
            }
            else if (lineBufPos < lineBufSize - 1)
            {
                lineBuf[lineBufPos++] = c;
            }
        }
        return false;
    }

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
        // Enable UART0 TX and RX so the hardware accepts incoming characters.
        Uart0().ctrl = txEn | rxEn;
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
        std::printf("\nCAN_TX %03lx %s\n", static_cast<unsigned long>(rawId), hexData);
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
        if (!DrainUartIntoLine())
            return;

        hal::Can::Id id{ hal::Can::Id::Create11BitId(0) };
        hal::Can::Message msg;
        if (!ParseCanLine(lineBuf, id, msg))
            return;

        if (receiveCallback)
            receiveCallback(id, msg);
    }
}
