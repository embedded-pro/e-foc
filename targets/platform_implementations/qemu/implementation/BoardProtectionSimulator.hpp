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

        // A real board's comparators feed the PWM fault input, so they only trip a drive that is
        // switching. Armed with the inverter, which also keeps a standing trip from firing at a
        // motor that was never enabled.
        void SetArmed(bool armed);

        // Callable from interrupt context: records the condition with plain word writes only.
        OPTIMIZE_FOR_SPEED void Evaluate(const foc::PhaseCurrents& currents, float busVoltageVolts, float windingTemperatureCelsius);

        // Runs on the event loop. A real board raises its protection on a dedicated fault
        // interrupt, not inside the control loop, and the handler stops the drive and walks the
        // state machine — far too much to run from the 20 kHz interrupt this model steps in.
        void DeliverPendingProtection();

        PlatformFactory::BoardProtectionState Status() const;

    private:
        OPTIMIZE_FOR_SPEED bool Exceeded(const foc::PhaseCurrents& currents, float busVoltageVolts,
            float windingTemperatureCelsius, PlatformFactory::BoardProtectionReason& reason) const;

        Trips trips;
        infra::Function<void(PlatformFactory::BoardProtectionReason)> onProtection;
        volatile bool armed{ false };
        volatile bool asserted{ false };
        // Separate from the condition itself: a trip that is already standing when the state
        // machine registers its handler must still be reported, not swallowed by an edge that
        // passed before anyone was listening.
        volatile bool notified{ false };
        volatile bool pending{ false };
        volatile PlatformFactory::BoardProtectionReason pendingReason{ PlatformFactory::BoardProtectionReason::overCurrent };
    };
}
