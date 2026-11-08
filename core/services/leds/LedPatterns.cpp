#include "core/services/leds/LedPatterns.hpp"

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

        LedPattern Heartbeat()
        {
            return LedPattern::Flashes(1, 1, 0, 39);
        }

        LedPattern SlowBlink()
        {
            return LedPattern::Flashes(1, 10, 10, 0);
        }

        LedPattern FaultCodeFlashes(uint8_t count)
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
            return lifecycle == LedLifecycle::enabled ? LedPattern::Steady() : Heartbeat();
        }

        LedPattern AmberFor(LedLifecycle lifecycle)
        {
            switch (lifecycle)
            {
                case LedLifecycle::idle:
                    return SlowBlink();
                case LedLifecycle::calibrating:
                    return LedPattern::Steady();
                default:
                    return LedPattern::Off();
            }
        }
    }

    LedPatterns PatternsFor(const LedStatus& status)
    {
        const auto red = status.lifecycle == LedLifecycle::fault ? RedFor(status.fault) : LedPattern::Off();
        return LedPatterns{ GreenFor(status.lifecycle), AmberFor(status.lifecycle), red };
    }
}
