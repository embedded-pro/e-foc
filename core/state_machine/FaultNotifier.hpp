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

    constexpr const char* FaultCodeName(FaultCode code)
    {
        switch (code)
        {
            case FaultCode::none:
                return "none";
            case FaultCode::overcurrent:
                return "overcurrent";
            case FaultCode::overvoltage:
                return "overvoltage";
            case FaultCode::undervoltage:
                return "undervoltage";
            case FaultCode::overtemperature:
                return "overtemperature";
            case FaultCode::encoderLoss:
                return "encoderLoss";
            case FaultCode::watchdogTimeout:
                return "watchdogTimeout";
            case FaultCode::hardwareFault:
                return "hardwareFault";
            case FaultCode::calibrationFailed:
                return "calibrationFailed";
        }

        return "unknown";
    }

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
