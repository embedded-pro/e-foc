#include "core/foc/model/ThermalModel.hpp"

namespace foc
{
    void ThermalModel::Configure(const ThermalConfig& newConfig)
    {
        config = newConfig;
    }

    void ThermalModel::Reset()
    {
        windingTempCelsius = config.ambientCelsius;
    }

    void ThermalModel::SetTemperature(float celsius)
    {
        windingTempCelsius = celsius;
    }

    float ThermalModel::Temperature() const
    {
        return windingTempCelsius;
    }

    foc::Ohm ThermalModel::Resistance(foc::Ohm atReference) const
    {
        return foc::Ohm{ atReference.Value() * (1.0f + config.copperTempCoeff * AboveReference()) };
    }

    foc::Henry ThermalModel::Inductance(foc::Henry atReference) const
    {
        return foc::Henry{ atReference.Value() * (1.0f - config.ironInductanceCoeff * AboveReference()) };
    }

    void ThermalModel::Step(float copperLossWatts, float dt)
    {
        windingTempCelsius += dt * (copperLossWatts - (windingTempCelsius - config.ambientCelsius) / config.thermalResistance) / config.thermalCapacitance;
    }

    float ThermalModel::AboveReference() const
    {
        return windingTempCelsius - config.referenceCelsius;
    }
}
