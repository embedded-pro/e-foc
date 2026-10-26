#include "targets/platform_implementations/qemu/implementation/CmsdkUartLines.hpp"

namespace
{
    constexpr uint32_t transmitterFull = 1u << 0;
    constexpr uint32_t receiverFull = 1u << 1;
    constexpr uint32_t transmitEnable = 1u << 0;
    constexpr uint32_t receiveEnable = 1u << 1;
}

namespace sil
{
    CmsdkUartLines::CmsdkUartLines(uintptr_t base)
        : registers(*reinterpret_cast<Registers*>(base))
    {
        registers.ctrl = transmitEnable | receiveEnable;
    }

    const char* CmsdkUartLines::PollLine()
    {
        while ((registers.state & receiverFull) != 0)
        {
            const auto c = static_cast<char>(registers.data & 0xffu);
            if (c != '\n' && c != '\r')
            {
                if (length < line.size() - 1)
                    line[length++] = c;
            }
            else if (length != 0)
            {
                line[length] = '\0';
                length = 0;
                return line.data();
            }
        }

        return nullptr;
    }

    void CmsdkUartLines::Write(std::string_view text) const
    {
        for (const auto c : text)
        {
            while ((registers.state & transmitterFull) != 0)
            {
            }

            registers.data = static_cast<uint8_t>(c);
        }
    }
}
