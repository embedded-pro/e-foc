#pragma once

#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include "core/foc/interfaces/Units.hpp"
#include <array>
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
            std::array<float, 3> targetCurrentFractions{ 0.3f, 0.5f, 0.7f };
            hal::Percent probeVoltagePercent{ 5 };
            infra::Duration settlePerLevel{ std::chrono::milliseconds{ 300 } };
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
