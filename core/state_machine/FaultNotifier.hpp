#pragma once

#include "infra/util/Function.hpp"
#include <cstdint>

namespace state_machine
{
    enum class FaultCode : uint8_t
    {
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
        virtual void Register(const infra::Function<void(FaultCode)>& onFault) = 0;
    };

    class NoOpFaultNotifier
        : public FaultNotifier
    {
    public:
        void Register(const infra::Function<void(FaultCode)>& onFault) override
        {}
    };
}
