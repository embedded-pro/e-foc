#include "tools/can_commander/logic/CommandTracker.hpp"
#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include <utility>

namespace tool
{
    CommandTracker::CommandTracker(const infra::Function<void(uint16_t nodeId, bool busy)>& onBusyChanged, const infra::Function<void(uint16_t nodeId, uint8_t commandType)>& onTimeout)
        : onBusyChanged{ onBusyChanged }
        , onTimeout{ onTimeout }
    {}

    infra::Duration CommandTracker::TimeoutFor(uint8_t commandType)
    {
        switch (commandType)
        {
            case can::focAlignId:
            case can::focIdentifyElectricalId:
            case can::focCalibrateId:
                return motionCommandTimeout;
            case can::focStartId:
                return startCommandTimeout;
            default:
                return commandTimeout;
        }
    }

    bool CommandTracker::IsBusy(uint16_t nodeId) const
    {
        const auto node = commands.find(nodeId);
        return node != commands.end() && node->second.busy;
    }

    void CommandTracker::MarkBusy(uint16_t nodeId, uint8_t commandType)
    {
        auto& node = commands[nodeId];
        node.pendingCommand = commandType;
        node.timeout.Start(TimeoutFor(commandType), [this, nodeId, commandType]()
            {
                Clear(nodeId);
                onTimeout(nodeId, commandType);
            });

        if (!std::exchange(node.busy, true))
            onBusyChanged(nodeId, true);
    }

    void CommandTracker::Clear(uint16_t nodeId)
    {
        const auto node = commands.find(nodeId);
        if (node == commands.end() || !node->second.busy)
            return;

        node->second.busy = false;
        node->second.timeout.Cancel();
        onBusyChanged(nodeId, false);
    }

    void CommandTracker::Complete(uint16_t nodeId, uint8_t commandType)
    {
        const auto node = commands.find(nodeId);
        if (node != commands.end() && node->second.pendingCommand == commandType)
            Clear(nodeId);
    }

    void CommandTracker::ClearAll()
    {
        for (const auto& [nodeId, command] : commands)
            Clear(nodeId);
    }
}
