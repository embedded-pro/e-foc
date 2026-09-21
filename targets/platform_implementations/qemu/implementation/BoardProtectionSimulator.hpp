#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "infra/util/Function.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace application
{
    // Stands in for the ADC digital comparators a real board wires to the PWM fault inputs. It runs
    // in the emulated FOC interrupt, which is the same context the hardware comparator fires in, and
    // is deliberately outside the control loop's cycle budget.
    class BoardProtectionSimulator
    {
    public:
        // A non-positive threshold disables that protection.
        struct Trips
        {
            float overCurrentAmpere{ 0.0f };
            float overVoltageVolts{ 0.0f };
            float underVoltageVolts{ 0.0f };
            float overTemperatureCelsius{ 0.0f };
        };

        void Configure(const Trips& trips);
        void Register(const infra::Function<void(PlatformFactory::BoardProtectionReason)>& onProtection);

        // Callable from interrupt context; notifies only on the clear-to-asserted edge, as the
        // hardware comparator interrupt does.
        OPTIMIZE_FOR_SPEED void Evaluate(const foc::PhaseCurrents& currents, float busVoltageVolts, float windingTemperatureCelsius);

        PlatformFactory::BoardProtectionState Status() const;

    private:
        OPTIMIZE_FOR_SPEED bool Exceeded(const foc::PhaseCurrents& currents, float busVoltageVolts,
            float windingTemperatureCelsius, PlatformFactory::BoardProtectionReason& reason) const;

        Trips trips;
        infra::Function<void(PlatformFactory::BoardProtectionReason)> onProtection;
        volatile bool asserted{ false };
    };
}
