#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/BoundedDeque.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/SharedPtr.hpp"
#include <cstdint>
#include <optional>

namespace services
{
    class ResistanceEstimator
        : public infra::EnableSharedFromThis<ResistanceEstimator>
    {
    public:
        struct Config
        {
            hal::DutyCycle testVoltage{ hal::DutyCycle::FromPercent(15) };
            infra::Duration settleTime{ std::chrono::seconds{ 2 } };
            WindingConfiguration windingConfig{ WindingConfiguration::Wye };
            infra::Duration noSampleTimeout{ std::chrono::milliseconds{ 100 } };
        };

        struct Result
        {
            std::optional<foc::Ohm> resistance;
        };

        ResistanceEstimator(drivers::ThreePhaseInverter& driver, foc::Volts vdc);
        ~ResistanceEstimator();

        void Start(const Config& config, const infra::Function<void(Result)>& onDone);

        void Abort();

    private:
        void StartSettlePhase();
        void StartMeasurementPhase();
        void OnMeasurementSample(foc::PhaseCurrents currents);
        void StartSampleWatchdog();
        void FailMeasurement();
        void Finish(bool measured);
        void Complete();
        Result Measure() const;

        static constexpr hal::DutyCycle neutralDuty{ hal::DutyCycle::FromPercent(1) };
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
        infra::TimerRepeating noSampleTimer;
        volatile bool sampleSeen{ false };
        volatile bool finishing{ false };
        bool measurementComplete{ false };
        uint32_t run{ 0 };
    };
}
