#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/current_offsets/PhaseCurrentOffsets.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/HotSwapFunction.hpp"
#include "infra/util/SharedPtr.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace services
{
    class PhaseCurrentOffsetCompensation
        : public drivers::ThreePhaseInverter
        , public PhaseCurrentOffsets
        , public infra::EnableSharedFromThis<PhaseCurrentOffsetCompensation>
    {
    public:
        struct Config
        {
            std::size_t settleSamples{ 200 };
            std::size_t averagedSamples{ 2000 };
            foc::Ampere maxOffset{ 0.5f };
            infra::Duration noSampleTimeout{ std::chrono::milliseconds{ 100 } };
        };

        explicit PhaseCurrentOffsetCompensation(drivers::ThreePhaseInverter& inverter);
        PhaseCurrentOffsetCompensation(drivers::ThreePhaseInverter& inverter, const Config& config);
        ~PhaseCurrentOffsetCompensation();

        void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

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

        OPTIMIZE_FOR_SPEED void ForwardCorrected(const foc::PhaseCurrents& currents)
        {
            onCorrectedCurrents(foc::PhaseCurrents{
                foc::Ampere{ currents.a.Value() - offsets.a.Value() },
                foc::Ampere{ currents.b.Value() - offsets.b.Value() },
                foc::Ampere{ currents.c.Value() - offsets.c.Value() } });
        }

        void OnMeasurementSample(foc::PhaseCurrents currents);
        void StartSampleWatchdog();
        void FailMeasurement();
        void Finish(bool measured);
        void Complete();
        std::optional<foc::PhaseCurrents> MeasuredOffsets() const;

        static constexpr hal::DutyCycle neutralDuty{ hal::DutyCycle::FromPercent(50) };

        drivers::ThreePhaseInverter& inverter;
        Config config;
        foc::PhaseCurrents offsets{};
        infra::HotSwapFunction<void(foc::PhaseCurrents)> onCorrectedCurrents;

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
