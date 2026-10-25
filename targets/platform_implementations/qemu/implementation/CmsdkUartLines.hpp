#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sil
{
    class CmsdkUartLines
    {
    public:
        explicit CmsdkUartLines(uintptr_t base);

        const char* PollLine();
        void Write(std::string_view text) const;

    private:
        struct Registers
        {
            volatile uint32_t data;
            volatile uint32_t state;
            volatile uint32_t ctrl;
            volatile uint32_t intStatus;
            volatile uint32_t baudDiv;
        };

        Registers& registers;
        std::array<char, 64> line{};
        std::size_t length{ 0 };
    };
}
