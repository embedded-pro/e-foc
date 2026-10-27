#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/current_offsets/PhaseCurrentOffsets.hpp"
#include "core/services/inverter_compensation/InverterCompensation.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/SharedPtr.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace services
{
    class PhaseCurrentOffsetMeasurement
        : public PhaseCurrentOffsets
        , public infra::EnableSharedFromThis<PhaseCurrentOffsetMeasurement>
    {
    public:
        struct Config
        {
            std::size_t settleSamples{ 200 };
            std::size_t averagedSamples{ 2000 };
            foc::Ampere maxOffset{ 0.5f };
            infra::Duration noSampleTimeout{ std::chrono::milliseconds{ 100 } };
        };

        PhaseCurrentOffsetMeasurement(drivers::ThreePhaseInverter& inverter, InverterCompensation& compensation);
        PhaseCurrentOffsetMeasurement(drivers::ThreePhaseInverter& inverter, InverterCompensation& compensation, const Config& config);
        ~PhaseCurrentOffsetMeasurement();

        void Measure(const infra::Function<void(std::optional<foc::PhaseCurrents>)>& onDone) override;
        void Apply(const foc::PhaseCurrents& offsets) override;
        bool IsRunning() const override;
        void Abort() override;

    private:
        struct Sums
        {
            float a{ 0.0f };
            float b{ 0.0f };
            float c{ 0.0f };
        };

        void OnMeasurementSample(foc::PhaseCurrents currents);
        void StartSampleWatchdog();
        void FailMeasurement();
        void Finish(bool measured);
        void Complete();
        std::optional<foc::PhaseCurrents> MeasuredOffsets() const;

        static constexpr hal::DutyCycle neutralDuty{ hal::DutyCycle::FromPercent(50) };

        drivers::ThreePhaseInverter& inverter;
        InverterCompensation& compensation;
        Config config;

        infra::AutoResetFunction<void(std::optional<foc::PhaseCurrents>)> onMeasured;
        infra::TimerRepeating noSampleTimer;
        Sums sums;
        std::size_t settledSamples{ 0 };
        std::size_t averagedSamples{ 0 };
        volatile bool sampleSeen{ false };
        volatile bool finishing{ false };
        bool measurementComplete{ false };
        uint32_t run{ 0 };
    };
}
