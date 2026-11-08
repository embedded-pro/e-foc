#include "core/services/leds/LedPattern.hpp"

namespace services
{
    namespace
    {
        constexpr uint16_t flashTicks = 5;
        constexpr uint16_t faultPauseTicks = 30;
        constexpr uint8_t overcurrentFlashes = 2;
        constexpr uint8_t overvoltageFlashes = 3;
        constexpr uint8_t overtemperatureFlashes = 4;
        constexpr uint8_t sensorFlashes = 5;
        constexpr uint8_t undervoltageFlashes = 6;
        constexpr uint8_t motionFlashes = 7;
        constexpr uint8_t commandTimeoutFlashes = 8;

        constexpr LedPattern heartbeat = LedPattern::Flashes(1, 1, 0, 39);
        constexpr LedPattern slowBlink = LedPattern::Flashes(1, 10, 10, 0);

        constexpr LedPattern FaultCodeFlashes(uint8_t count)
        {
            return LedPattern::Flashes(count, flashTicks, flashTicks, faultPauseTicks);
        }

        LedPattern RedFor(state_machine::FaultCode code)
        {
            using state_machine::FaultCode;
            switch (code)
            {
                case FaultCode::overcurrent:
                    return FaultCodeFlashes(overcurrentFlashes);
                case FaultCode::overvoltage:
                    return FaultCodeFlashes(overvoltageFlashes);
                case FaultCode::overtemperature:
                    return FaultCodeFlashes(overtemperatureFlashes);
                case FaultCode::encoderLoss:
                    return FaultCodeFlashes(sensorFlashes);
                case FaultCode::undervoltage:
                    return FaultCodeFlashes(undervoltageFlashes);
                case FaultCode::stall:
                case FaultCode::phaseMissing:
                case FaultCode::overspeed:
                case FaultCode::followingError:
                case FaultCode::positionLimit:
                    return FaultCodeFlashes(motionFlashes);
                case FaultCode::commandTimeout:
                    return FaultCodeFlashes(commandTimeoutFlashes);
                default:
                    return LedPattern::Steady();
            }
        }

        LedPattern GreenFor(LedLifecycle lifecycle)
        {
            if (lifecycle == LedLifecycle::fault)
                return LedPattern::Off();
            return lifecycle == LedLifecycle::enabled ? LedPattern::Steady() : heartbeat;
        }

        LedPattern AmberFor(LedLifecycle lifecycle)
        {
            switch (lifecycle)
            {
                case LedLifecycle::idle:
                    return slowBlink;
                case LedLifecycle::calibrating:
                    return LedPattern::Steady();
                default:
                    return LedPattern::Off();
            }
        }
    }

    uint32_t LedPattern::PeriodTicks() const
    {
        if (kind != Kind::flashes)
            return 1;
        return static_cast<uint32_t>(flashes) * (onTicks + offTicks) + pauseTicks;
    }

    bool LedPattern::LevelAt(uint32_t tick) const
    {
        if (kind != Kind::flashes)
            return kind == Kind::steady;

        const uint32_t flashLength = static_cast<uint32_t>(onTicks) + offTicks;
        const uint32_t position = tick % PeriodTicks();
        if (position >= flashes * flashLength)
            return false;
        return position % flashLength < onTicks;
    }

    LedPatterns PatternsFor(const LedStatus& status)
    {
        const auto red = status.lifecycle == LedLifecycle::fault ? RedFor(status.fault) : LedPattern::Off();
        return LedPatterns{ GreenFor(status.lifecycle), AmberFor(status.lifecycle), red };
    }
}
