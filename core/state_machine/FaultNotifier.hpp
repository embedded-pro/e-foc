#pragma once

#include "infra/util/Function.hpp"
#include <cstdint>

namespace state_machine
{
    enum class FaultCode : uint8_t
    {
        none,
        overcurrent,
        overvoltage,
        overtemperature,
        encoderLoss,
        watchdogTimeout,
        hardwareFault,
        calibrationFailed
    };

    class FaultNotifier
    {
    public:
        virtual ~FaultNotifier();
        virtual void Register(const infra::Function<void(FaultCode)>& onFault) = 0;

        // A notifier outlives the state machines that register with it - a mode switch destroys one
        // and constructs another - so a registration that is not released dangles into freed storage.
        virtual void Unregister() = 0;
    };

    class NoOpFaultNotifier
        : public FaultNotifier
    {
    public:
        ~NoOpFaultNotifier() override;

        void Register(const infra::Function<void(FaultCode)>&) override
        {}

        void Unregister() override
        {}
    };
}
