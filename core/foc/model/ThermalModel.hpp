#pragma once

#include "core/foc/interfaces/Units.hpp"

namespace foc
{
    struct ThermalConfig
    {
        float ambientCelsius{ 25.0f };
        float thermalResistance{ 2.0f };
        float thermalCapacitance{ 25.0f };
        float copperTempCoeff{ 0.00393f };
        float ironInductanceCoeff{ 0.0f };
        float referenceCelsius{ 25.0f };
    };

    class ThermalModel
    {
    public:
        void Configure(const ThermalConfig& config);
        void Reset();
        void SetTemperature(float celsius);
        float Temperature() const;

        foc::Ohm Resistance(foc::Ohm atReference) const;
        foc::Henry Inductance(foc::Henry atReference) const;

        void Step(float copperLossWatts, float dt);

    private:
        float AboveReference() const;

    private:
        ThermalConfig config{};
        float windingTempCelsius{ 25.0f };
    };
}
