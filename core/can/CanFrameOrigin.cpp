#include "core/can/CanFrameOrigin.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"

namespace can
{
    CanFrameOriginTracker::CanFrameOriginTracker(hal::Can& can)
        : can(can)
    {}

    void CanFrameOriginTracker::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        can.SendData(id, data, actionOnCompletion);
    }

    void CanFrameOriginTracker::ReceiveData(const infra::Function<void(Id id, const Message& data)>& action)
    {
        receivedAction = action;

        if (!receivedAction)
        {
            can.ReceiveData(nullptr);
            return;
        }

        can.ReceiveData([this](Id id, const Message& data)
            {
                sourceNodeId = id.Is29BitId() ? static_cast<uint16_t>(services::ExtractCanNodeId(id.Get29BitId())) : uint16_t{ 0 };
                receivedAction(id, data);
            });
    }

    uint16_t CanFrameOriginTracker::SourceNodeId() const
    {
        return sourceNodeId;
    }
}
