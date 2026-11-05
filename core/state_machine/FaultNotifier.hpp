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
        hardwareFault,
        calibrationFailed,
        stall,
        phaseMissing,
        overspeed,
        followingError
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
            case FaultCode::hardwareFault:
                return "hardwareFault";
            case FaultCode::calibrationFailed:
                return "calibrationFailed";
            case FaultCode::stall:
                return "stall";
            case FaultCode::phaseMissing:
                return "phaseMissing";
            case FaultCode::overspeed:
                return "overspeed";
            case FaultCode::followingError:
                return "followingError";
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
    protected:
        ~FaultNotifier() = default;

    public:
        virtual void Register(const infra::Function<void(FaultCode)>& onImmediate, const infra::Function<void(FaultCode)>& onDeferred) = 0;

        virtual void Unregister() = 0;

        virtual FaultConditionState ConditionState() = 0;
    };

    class NoOpFaultNotifier
        : public FaultNotifier
    {
    public:
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
