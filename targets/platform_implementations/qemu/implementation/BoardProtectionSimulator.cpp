#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "targets/platform_implementations/qemu/implementation/BoardProtectionSimulator.hpp"
#include <cmath>

namespace application
{
    void BoardProtectionSimulator::Configure(const Trips& configuredTrips)
    {
        trips = configuredTrips;
        asserted = false;
        notified = false;
        pending = false;
    }

    void BoardProtectionSimulator::SetArmed(bool isArmed)
    {
        armed = isArmed;
        if (!isArmed)
        {
            asserted = false;
            notified = false;
            pending = false;
        }
    }

    void BoardProtectionSimulator::Register(const infra::Function<void(PlatformFactory::BoardProtectionReason)>& handler)
    {
        onProtection = handler;
    }

    PlatformFactory::BoardProtectionState BoardProtectionSimulator::Status() const
    {
        return asserted ? PlatformFactory::BoardProtectionState::asserted
                        : PlatformFactory::BoardProtectionState::clear;
    }

    OPTIMIZE_FOR_SPEED bool BoardProtectionSimulator::Exceeded(const foc::PhaseCurrents& currents,
        float busVoltageVolts, float windingTemperatureCelsius,
        PlatformFactory::BoardProtectionReason& reason) const
    {
        if (trips.overCurrentAmpere > 0.0f)
        {
            const auto peak = std::fmax(std::fmax(std::fabs(currents.a.Value()), std::fabs(currents.b.Value())), std::fabs(currents.c.Value()));
            if (peak > trips.overCurrentAmpere)
            {
                reason = PlatformFactory::BoardProtectionReason::overCurrent;
                return true;
            }
        }

        if (trips.overVoltageVolts > 0.0f && busVoltageVolts > trips.overVoltageVolts)
        {
            reason = PlatformFactory::BoardProtectionReason::overVoltage;
            return true;
        }

        if (trips.underVoltageVolts > 0.0f && busVoltageVolts < trips.underVoltageVolts)
        {
            reason = PlatformFactory::BoardProtectionReason::underVoltage;
            return true;
        }

        if (trips.overTemperatureCelsius > 0.0f && windingTemperatureCelsius > trips.overTemperatureCelsius)
        {
            reason = PlatformFactory::BoardProtectionReason::overTemperature;
            return true;
        }

        return false;
    }

    OPTIMIZE_FOR_SPEED void BoardProtectionSimulator::Evaluate(const foc::PhaseCurrents& currents,
        float busVoltageVolts, float windingTemperatureCelsius)
    {
        if (!armed)
            return;

        auto reason = PlatformFactory::BoardProtectionReason::overCurrent;
        const bool exceeded = Exceeded(currents, busVoltageVolts, windingTemperatureCelsius, reason);
        asserted = exceeded;

        if (!exceeded)
        {
            notified = false;
            return;
        }

        if (!notified)
        {
            pendingReason = reason;
            pending = true;
        }
    }

    void BoardProtectionSimulator::DeliverPendingProtection()
    {
        if (!pending || !onProtection)
            return;

        pending = false;
        notified = true;
        onProtection(pendingReason);
    }
}
