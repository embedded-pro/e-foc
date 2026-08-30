#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/BoundedDeque.hpp"
#include "infra/util/BoundedVector.hpp"
#include <optional>

namespace services
{
    class ResistanceEstimator
    {
    public:
        struct Config
        {
            hal::Percent testVoltagePercent{ 15 };
            infra::Duration settleTime{ std::chrono::seconds{ 2 } };
            WindingConfiguration windingConfig{ WindingConfiguration::Wye };
        };

        struct Result
        {
            std::optional<foc::Ohm> resistance;
        };

        ResistanceEstimator(drivers::ThreePhaseInverter& driver, foc::Volts vdc);

        void Start(const Config& config, const infra::Function<void(Result)>& onDone);

    private:
        void OnMeasurementComplete();

        static constexpr uint8_t neutralDuty = 1;
        static constexpr float wyeTerminalFactor = 1.5f;
        static constexpr float deltaTerminalFactor = 0.5f;
        static constexpr std::size_t averageFilterSize = 5;
        static constexpr std::size_t steadyStateSamplesSize = 123;

        drivers::ThreePhaseInverter& driver;
        foc::Volts vdc;

        Config activeConfig;
        infra::AutoResetFunction<void(Result)> onDone;
        infra::BoundedDeque<float>::WithMaxSize<averageFilterSize> currentSamples;
        infra::BoundedVector<float>::WithMaxSize<steadyStateSamplesSize> filteredSamples;
        infra::TimerSingleShot settleTimer;
    };
}
