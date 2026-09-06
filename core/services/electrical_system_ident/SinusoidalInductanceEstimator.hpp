#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "numerical/analysis/GoertzelAlgorithm.hpp"
#include <optional>

namespace services
{
    class SinusoidalInductanceEstimator
    {
    public:
        struct Config
        {
            hal::Hertz injectionFrequency{ 700 };
            hal::Percent injectionVoltagePercent{ 15 };
            std::size_t warmupPeriods{ 5 };
            std::size_t measurementPeriods{ 20 };
            std::size_t voltageToCurrentDelaySamples{ 1 };
            WindingConfiguration windingConfig{ WindingConfiguration::Wye };
        };

        struct Result
        {
            std::optional<foc::MilliHenry> inductance;
            float fitQuality{ 0.0f };
        };

        SinusoidalInductanceEstimator(drivers::ThreePhaseInverter& driver, foc::Volts vdc);

        void Start(const Config& config, const infra::Function<void(Result)>& onDone);

        // Stops injection and drops the pending completion without invoking it.
        void Abort();

    private:
        void OnCurrentSample(foc::PhaseCurrents currents);
        Result ComputeResult() const;

        static constexpr float wyeTerminalFactor = 1.5f;
        static constexpr float deltaTerminalFactor = 0.5f;

        drivers::ThreePhaseInverter& driver;
        foc::Volts vdc;
        [[no_unique_address]] foc::ClarkePark transforms;

        Config activeConfig;
        infra::AutoResetFunction<void(Result)> onDone;

        float injectionPhase{ 0.0f };
        float phaseIncrement{ 0.0f };
        float injectionAmplitude{ 0.0f };
        float vTerminalAmplitude{ 0.0f };
        float omega{ 0.0f };
        float terminalFactor{ wyeTerminalFactor };
        std::size_t warmupSamples{ 0 };
        std::size_t measurementSamples{ 0 };
        std::size_t sampleCount{ 0 };
        float sumSquared{ 0.0f };

        std::optional<analysis::GoertzelAlgorithm<float>> goertzel;
    };
}
