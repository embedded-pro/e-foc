#pragma once

#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>

namespace tool
{
    struct NodeRead
    {
        uint8_t commandType{ 0 };
        can::FocParameter parameter{ can::FocParameter::currentBandwidth };
        uint8_t age{ 0 };

        bool operator==(const NodeRead& other) const = default;
    };

    // Keeps one read in flight per node; the next goes out once the node answers or the read times out.
    class ReadScheduler
    {
    public:
        static constexpr infra::Duration readTimeout{ std::chrono::seconds(1) };

        explicit ReadScheduler(const infra::Function<bool(uint16_t nodeId, const NodeRead& read)>& send);

        static bool IsRead(uint8_t commandType);

        void Queue(uint16_t nodeId, const NodeRead& read);
        void Complete(uint16_t nodeId, uint8_t commandType);
        void Forget(uint16_t nodeId);
        std::optional<can::FocParameter> InFlightParameter(uint16_t nodeId, uint8_t commandType) const;

        template<class Predicate>
        bool AnyQueued(uint16_t nodeId, Predicate predicate) const
        {
            const auto node = reads.find(nodeId);
            return node != reads.end() && std::ranges::any_of(node->second.queued, predicate);
        }

    private:
        struct NodeReads
        {
            std::deque<NodeRead> queued;
            std::optional<NodeRead> inFlight;
            infra::TimerSingleShot timeout;
        };

        void SendNext(uint16_t nodeId);

        infra::Function<bool(uint16_t, const NodeRead&)> send;
        std::map<uint16_t, NodeReads> reads;
    };
}
