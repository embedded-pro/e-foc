#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/BoundedDeque.hpp"
#include "infra/util/Function.hpp"
#include "source/services/can_protocol/core/CanProtocolDefinitions.hpp"
#include <cstdint>

namespace services
{
    class CanFrameTransport
    {
    public:
        CanFrameTransport(hal::Can& can, uint16_t nodeId);

        void SetNodeId(uint16_t nodeId);
        uint16_t NodeId() const;

        void SendFrame(CanPriority priority, CanCategory category, CanMessageType type,
            const hal::Can::Message& data, const infra::Function<void()>& onDone);

    private:
        struct PendingFrame
        {
            hal::Can::Id id;
            hal::Can::Message data;
            infra::Function<void()> onDone;

            PendingFrame(hal::Can::Id id, const hal::Can::Message& data, const infra::Function<void()>& onDone)
                : id(id)
                , data(data)
                , onDone(onDone)
            {}

            PendingFrame(PendingFrame&&) noexcept = default;
            PendingFrame& operator=(PendingFrame&&) noexcept = default;
            PendingFrame(const PendingFrame&) = default;
            PendingFrame& operator=(const PendingFrame&) = default;
        };

        void SendNextQueued();

        hal::Can& can;
        uint16_t nodeId;
        bool sendInProgress = false;
        infra::Function<void()> currentOnDone;
        infra::BoundedDeque<PendingFrame>::WithMaxSize<8> sendQueue;
    };
}
