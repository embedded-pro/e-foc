#pragma once

#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/BoundedVector.hpp"
#include "numerical/estimators/offline/LinearRegression.hpp"
#include "core/foc/implementations/TransformsClarkePark.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include <array>
#include <tuple>

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
        void StartProbeStep();
        void OnProbeBufferFull();
        void StartLevel(std::size_t level);
        void OnLevelBatchFull(std::size_t level);
        bool FitResistance();
        float ResistanceFitResidual() const;
        void ComputeAndReport();
        void ApplyNextElectricalAngle();
        void RunPolePairLogic();
        void CalculatePolePairs();

        static constexpr uint8_t neutralDuty = 1;
        static constexpr float deltaCoefficient = 1.5f;
        static constexpr std::size_t numLevels = 3;
        static constexpr std::size_t probeBufferSize = 512;
        static constexpr std::size_t steadyStateSamples = 32;
        static constexpr float maxAcceptableFitResidual = 0.1f;

        static_assert(numLevels == std::tuple_size<decltype(ResistanceAndInductanceConfig::targetCurrentFractions)>::value, "numLevels must match the size of ResistanceAndInductanceConfig::targetCurrentFractions");

        foc::ThreePhaseInverter& driver;
        foc::Encoder& encoder;
        foc::Volts vdc;
        [[no_unique_address]] foc::ClarkePark transforms;
        ResistanceAndInductanceConfig rlConfig;
        PolePairsConfig polePairsConfig;

        infra::BoundedVector<float>::WithMaxSize<probeBufferSize> probeBuffer;
        infra::BoundedVector<float>::WithMaxSize<steadyStateSamples> levelBatch;

        float rCoarse{ 0.0f };
        std::array<float, numLevels> levelVoltages{};
        std::array<float, numLevels> levelSteadyStateCurrents{};
        float fittedResistance{ 0.0f };
        float fittedVoltageOffset{ 0.0f };

        std::size_t currentSampleIndex{ 0 };
        foc::Radians initialPosition{ 0.0f };
        foc::Radians previousPosition{ 0.0f };
        float accumulatedRotation{ 0.0f };

        infra::AutoResetFunction<void(std::optional<ResistanceInductanceResult>)> onResistanceAndInductanceDone;
        infra::AutoResetFunction<void(std::optional<std::size_t>)> onPolePairsDone;

        infra::TimerSingleShot settleTimer;
    };
}
