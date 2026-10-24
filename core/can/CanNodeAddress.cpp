#include "core/can/CanNodeAddress.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"

namespace can
{
    uint16_t CanNodeId(uint32_t configuredBase, uint8_t boardId)
    {
        const auto fits = configuredBase > services::canBroadcastNodeId && configuredBase <= services::canIdNodeIdMask - boardId;
        return static_cast<uint16_t>((fits ? configuredBase : defaultCanNodeIdBase) + boardId);
    }
}
