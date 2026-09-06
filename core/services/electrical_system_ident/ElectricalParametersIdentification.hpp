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
        virtual ~ElectricalParametersIdentification() = default;

        struct ResistanceAndInductanceConfig
        {
            hal::Percent testVoltagePercent{ 15 };
            infra::Duration settleTime{ std::chrono::seconds{ 2 } };
            WindingConfiguration windingConfig{ WindingConfiguration::Wye };
            hal::Hertz injectionFrequency{ 700 };
            hal::Percent injectionVoltagePercent{ 15 };
            std::size_t warmupPeriods{ 5 };
            std::size_t measurementPeriods{ 20 };
            std::size_t voltageToCurrentDelaySamples{ 1 };
        };

        struct ResistanceInductanceResult
        {
            std::optional<foc::Ohm> resistance;
            std::optional<foc::MilliHenry> inductance;
            float fitQuality{ 0.0f };
        };

        struct PolePairsConfig
        {
            hal::Percent testVoltagePercent{ 10 };
            std::size_t electricalRevolutions{ 5 };
            infra::Duration settleTimeBetweenSteps{ std::chrono::milliseconds{ 50 } };
        };

        virtual void EstimateResistanceAndInductance(const ResistanceAndInductanceConfig& config, const infra::Function<void(ResistanceInductanceResult)>& onDone) = 0;
        virtual void EstimateNumberOfPolePairs(const PolePairsConfig& config, const infra::Function<void(std::optional<std::size_t>)>& onDone) = 0;
    };
}
