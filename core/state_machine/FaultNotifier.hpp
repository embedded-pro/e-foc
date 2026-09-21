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
        undervoltage,
        overtemperature,
        encoderLoss,
        watchdogTimeout,
        hardwareFault,
        calibrationFailed
    };

    enum class FaultConditionState : uint8_t
    {
        clear,
        asserted,
        unknown
    };

    class FaultNotifier
    {
    public:
        virtual ~FaultNotifier() = default;
        virtual void Register(const infra::Function<void(FaultCode)>& onImmediate, const infra::Function<void(FaultCode)>& onDeferred) = 0;

        virtual void Unregister() = 0;

        virtual FaultConditionState ConditionState() = 0;
    };

    class NoOpFaultNotifier
        : public FaultNotifier
    {
    public:
        ~NoOpFaultNotifier() override = default;

        void Register(const infra::Function<void(FaultCode)>&, const infra::Function<void(FaultCode)>&) override
        {}

        void Unregister() override
        {}

        FaultConditionState ConditionState() override
        {
            return FaultConditionState::unknown;
        }
    };
}
