#pragma once

#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include "source/foc/interfaces/Units.hpp"
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
            hal::Percent testVoltagePercent{ 15 };
            infra::Duration settleTime{ std::chrono::seconds{ 2 } };
            WindingConfiguration windingConfig{ WindingConfiguration::Wye };
        };

        struct PolePairsConfig
        {
            hal::Percent testVoltagePercent{ 10 };
            std::size_t electricalRevolutions{ 5 };
            infra::Duration settleTimeBetweenSteps{ std::chrono::milliseconds{ 50 } };
        };

        virtual void EstimateResistanceAndInductance(const ResistanceAndInductanceConfig& config, const infra::Function<void(std::optional<foc::Ohm>, std::optional<foc::MilliHenry>)>& onDone) = 0;
        virtual void EstimateNumberOfPolePairs(const PolePairsConfig& config, const infra::Function<void(std::optional<std::size_t>)>& onDone) = 0;
    };
}
