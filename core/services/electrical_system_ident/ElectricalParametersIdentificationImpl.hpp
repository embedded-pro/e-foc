#pragma once

#include "core/foc/implementations/TransformsClarkePark.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"

namespace services
{
    class ElectricalParametersIdentificationImpl
        : public ElectricalParametersIdentification
    {
    public:
        ElectricalParametersIdentificationImpl(foc::ThreePhaseInverter& driver, foc::Encoder& encoder, foc::Volts vdc);

        void EstimateResistanceAndInductance(const ResistanceAndInductanceConfig& config, const infra::Function<void(std::optional<ResistanceInductanceResult>)>& onDone) override;
        void EstimateNumberOfPolePairs(const PolePairsConfig& config, const infra::Function<void(std::optional<std::size_t>)>& onDone) override;

    private:
        void ApplyInjectionVoltage();
        void OnHfSample(const foc::PhaseCurrents& currentPhases);
        void AbortResistanceAndInductance();
        void ComputeAndReport();
        void ApplyNextElectricalAngle();
        void RunPolePairLogic();
        void CalculatePolePairs();

        static constexpr float deltaCoefficient = 1.5f;
        static constexpr float minDemodulatedCurrent = 0.05f;
        static constexpr std::size_t samplingFrequencyHz = 10000;

        foc::ThreePhaseInverter& driver;
        foc::Encoder& encoder;
        foc::Volts vdc;
        [[no_unique_address]] foc::Clarke clarke;
        [[no_unique_address]] foc::ClarkePark transforms;
        ResistanceAndInductanceConfig rlConfig;
        PolePairsConfig polePairsConfig;

        float injectionModIndex{ 0.0f };
        float phase{ 0.0f };
        float demodPhase{ 0.0f };
        float phaseIncrement{ 0.0f };
        float angularFrequency{ 0.0f };
        float maxCurrentSquared{ 0.0f };
        std::size_t sampleIndex{ 0 };
        std::size_t warmupSamples{ 0 };
        std::size_t measurementSamples{ 0 };
        float sumSin{ 0.0f };
        float sumCos{ 0.0f };
        float sumSq{ 0.0f };

        std::size_t currentSampleIndex{ 0 };
        foc::Radians previousPosition{ 0.0f };
        float accumulatedRotation{ 0.0f };

        infra::AutoResetFunction<void(std::optional<ResistanceInductanceResult>)> onResistanceAndInductanceDone;
        infra::AutoResetFunction<void(std::optional<std::size_t>)> onPolePairsDone;

        infra::TimerSingleShot settleTimer;
    };
}
