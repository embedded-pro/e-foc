#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <optional>

namespace services
{
    enum class WindingConfiguration
    {
        Wye,
        Delta
    };

    class ElectricalParametersIdentification
    {
    public:
        struct ResistanceAndInductanceConfig
        {
            hal::Hertz injectionFrequency{ 250 };       // must divide the sampling frequency (10 kHz)
            hal::Percent injectionVoltagePercent{ 15 }; // peak alpha modulation; clamped so duty stays samplable
            std::size_t warmupPeriods{ 10 };
            std::size_t measurementPeriods{ 50 };
            std::size_t voltageToCurrentDelaySamples{ 1 }; // PWM->ADC pipeline lag; rig-calibrated (see theory doc)
            WindingConfiguration windingConfig{ WindingConfiguration::Wye };
        };

        struct ResistanceInductanceResult
        {
            foc::Ohm resistance;
            foc::MilliHenry inductance;
            foc::Volts inverterVoltageOffset;
            float fitQuality;
        };

        struct PolePairsConfig
        {
            hal::Percent testVoltagePercent{ 10 };
            std::size_t electricalRevolutions{ 5 };
            infra::Duration settleTimeBetweenSteps{ std::chrono::milliseconds{ 50 } };
        };

        virtual void EstimateResistanceAndInductance(const ResistanceAndInductanceConfig& config, const infra::Function<void(std::optional<ResistanceInductanceResult>)>& onDone) = 0;
        virtual void EstimateNumberOfPolePairs(const PolePairsConfig& config, const infra::Function<void(std::optional<std::size_t>)>& onDone) = 0;
    };
}
