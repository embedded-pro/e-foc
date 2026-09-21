#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "infra/util/Function.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace application
{
    class BoardProtectionSimulator
    {
    public:
        struct Trips
        {
            float overCurrentAmpere{ 0.0f };
            float overVoltageVolts{ 0.0f };
            float underVoltageVolts{ 0.0f };
            float overTemperatureCelsius{ 0.0f };
        };

        void Configure(const Trips& trips);
        void Register(const infra::Function<void(PlatformFactory::BoardProtectionReason)>& onProtection);

        void SetArmed(bool armed);

        OPTIMIZE_FOR_SPEED void Evaluate(const foc::PhaseCurrents& currents, float busVoltageVolts, float windingTemperatureCelsius);

        void DeliverPendingProtection();

        PlatformFactory::BoardProtectionState Status() const;

    private:
        OPTIMIZE_FOR_SPEED bool Exceeded(const foc::PhaseCurrents& currents, float busVoltageVolts,
            float windingTemperatureCelsius, PlatformFactory::BoardProtectionReason& reason) const;

        Trips trips;
        infra::Function<void(PlatformFactory::BoardProtectionReason)> onProtection;
        volatile bool armed{ false };
        volatile bool asserted{ false };
        volatile bool notified{ false };
        volatile bool pending{ false };
        volatile PlatformFactory::BoardProtectionReason pendingReason{ PlatformFactory::BoardProtectionReason::overCurrent };
    };
}
