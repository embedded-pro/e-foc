#pragma once

#include "tools/virtual_motor/platform/PlantSignals.hpp"
#include <cstdint>
#include <optional>

namespace virtual_motor
{
    class StreamWindow
    {
    public:
        static constexpr uint8_t periodsPerWindow{ 10 };

        void Begin();
        std::optional<port::Window> Add(uint64_t period, const SignalSample& sample);

    private:
        port::Window window;
        uint32_t sequence{ 0 };
        uint8_t collected{ 0 };
    };
}
