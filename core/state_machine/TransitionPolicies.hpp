#pragma once

#include <cstdint>

namespace state_machine
{
    enum class TransitionPolicy : uint8_t
    {
        Cli,
        Auto
    };
}
