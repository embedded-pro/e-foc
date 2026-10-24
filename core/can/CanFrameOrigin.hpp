#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace can
{
    class CanFrameOrigin
    {
    public:
        virtual uint16_t SourceNodeId() const = 0;

    protected:
        CanFrameOrigin() = default;
        CanFrameOrigin(const CanFrameOrigin&) = delete;
        CanFrameOrigin& operator=(const CanFrameOrigin&) = delete;
        ~CanFrameOrigin() = default;
    };

    class CanFrameOriginTracker
        : public hal::Can
        , public CanFrameOrigin
    {
    public:
        explicit CanFrameOriginTracker(hal::Can& can);

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

        uint16_t SourceNodeId() const override;

    private:
        hal::Can& can;
        infra::Function<void(Id id, const Message& data)> receivedAction;
        uint16_t sourceNodeId{ 0 };
    };
}
