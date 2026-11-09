#include "tools/can_commander/logic/ReadScheduler.hpp"

namespace tool
{
    ReadScheduler::ReadScheduler(const infra::Function<bool(uint16_t nodeId, const NodeRead& read)>& send)
        : send{ send }
    {}

    bool ReadScheduler::IsRead(uint8_t commandType)
    {
        return commandType == can::focQueryAlgorithmsId || commandType == can::focQueryParameterId || commandType == can::focRequestTelemetryId ||
               commandType == can::focQueryFaultHistoryId;
    }

    void ReadScheduler::Queue(uint16_t nodeId, const NodeRead& read)
    {
        auto& node = reads[nodeId];
        if (std::ranges::find(node.queued, read) == node.queued.end())
            node.queued.push_back(read);

        if (!node.inFlight.has_value())
            SendNext(nodeId);
    }

    void ReadScheduler::SendNext(uint16_t nodeId)
    {
        auto& node = reads[nodeId];
        while (!node.queued.empty())
        {
            const auto read = node.queued.front();
            node.queued.pop_front();

            if (send(nodeId, read))
            {
                node.inFlight = read;
                node.timeout.Start(readTimeout, [this, nodeId, commandType = read.commandType]()
                    {
                        Complete(nodeId, commandType);
                    });
                return;
            }
        }
    }

    void ReadScheduler::Complete(uint16_t nodeId, uint8_t commandType)
    {
        const auto node = reads.find(nodeId);
        if (node == reads.end() || !node->second.inFlight.has_value() || node->second.inFlight->commandType != commandType)
            return;

        node->second.inFlight.reset();
        node->second.timeout.Cancel();
        SendNext(nodeId);
    }

    void ReadScheduler::Forget(uint16_t nodeId)
    {
        const auto node = reads.find(nodeId);
        if (node == reads.end())
            return;

        node->second.queued.clear();
        node->second.inFlight.reset();
        node->second.timeout.Cancel();
    }

    std::optional<can::FocParameter> ReadScheduler::InFlightParameter(uint16_t nodeId, uint8_t commandType) const
    {
        const auto node = reads.find(nodeId);
        if (node == reads.end() || !node->second.inFlight.has_value() || node->second.inFlight->commandType != commandType)
            return std::nullopt;

        return node->second.inFlight->parameter;
    }
}
