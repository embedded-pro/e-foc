#pragma once

#include <cstdint>

namespace can
{
    inline constexpr uint32_t defaultCanNodeIdBase{ 1 };

    uint16_t CanNodeId(uint32_t configuredBase, uint8_t boardId);
}
