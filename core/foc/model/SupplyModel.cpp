#include "core/foc/model/SupplyModel.hpp"
#include <algorithm>

namespace foc
{
    namespace
    {
        constexpr float bulkTimeConstantSeconds = 0.001f;
        constexpr float minimumBusVoltage = 1.0f;
    }

    SupplyModel::SupplyModel(foc::Volts openCircuitVoltage)
        : openCircuitVoltage(openCircuitVoltage)
    {}

    void SupplyModel::Configure(const SupplyConfig& newConfig)
    {
        config = newConfig;
        busCurrentAmpere = 0.0f;
    }

    void SupplyModel::SetVoltageScale(float scale)
    {
        voltageScale = scale;
    }

    foc::Volts SupplyModel::Voltage() const
    {
        const float open = openCircuitVoltage.Value() * voltageScale;
        float loaded = open - config.sourceResistanceOhm * busCurrentAmpere;

        if (config.currentLimitAmpere > 0.0f && busCurrentAmpere > config.currentLimitAmpere)
            loaded *= config.currentLimitAmpere / busCurrentAmpere;

        return foc::Volts{ std::max(loaded, 0.0f) };
    }

    foc::Ampere SupplyModel::Current() const
    {
        return foc::Ampere{ busCurrentAmpere };
    }

    void SupplyModel::Draw(float inverterPowerWatts, foc::Volts bus, float dt)
    {
        if (config.sourceResistanceOhm <= 0.0f && config.currentLimitAmpere <= 0.0f)
            return;

        const float instantaneous = std::max(inverterPowerWatts, 0.0f) / std::max(bus.Value(), minimumBusVoltage);
        busCurrentAmpere += (instantaneous - busCurrentAmpere) * std::min(dt / bulkTimeConstantSeconds, 1.0f);
    }

    void SupplyModel::Release()
    {
        busCurrentAmpere = 0.0f;
    }
}
