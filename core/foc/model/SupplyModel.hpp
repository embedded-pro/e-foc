#pragma once

#include "core/foc/interfaces/Units.hpp"

namespace foc
{
    struct SupplyConfig
    {
        float sourceResistanceOhm{ 0.0f };
        float currentLimitAmpere{ 0.0f };
    };

    class SupplyModel
    {
    public:
        explicit SupplyModel(foc::Volts openCircuitVoltage);

        void Configure(const SupplyConfig& config);
        void SetVoltageScale(float scale);

        foc::Volts Voltage() const;
        foc::Ampere Current() const;

        void Draw(float inverterPowerWatts, foc::Volts bus, float dt);
        void Release();

    private:
        foc::Volts openCircuitVoltage;
        SupplyConfig config{};
        float voltageScale{ 1.0f };
        float busCurrentAmpere{ 0.0f };
    };
}
