#pragma once

#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/BoundedDeque.hpp"
#include "infra/util/BoundedVector.hpp"
#include "source/foc/implementations/TransformsClarkePark.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/services/parameter_identification/ElectricalParametersIdentification.hpp"

namespace services
{
    class ElectricalParametersIdentificationImpl
        : public ElectricalParametersIdentification
    {
    public:
        ElectricalParametersIdentificationImpl(foc::MotorDriver& driver, foc::Encoder& encoder, foc::Volts vdc);

        void EstimateResistanceAndInductance(const ResistanceAndInductanceConfig& config, const infra::Function<void(std::optional<foc::Ohm>, std::optional<foc::MilliHenry>)>& onDone) override;
        void EstimateNumberOfPolePairs(const PolePairsConfig& config, const infra::Function<void(std::optional<std::size_t>)>& onDone) override;

    private:
        void AnalyzeInductanceMeasures();
        void CalculatePolePairs();
        void ApplyNextElectricalAngle();
        void RunPolePairLogic();

        constexpr static uint8_t neutralDuty = 1;
        constexpr static float deltaCoefficient = 1.5f;
        constexpr static std::size_t inductanceSamplesSize = 128;
        constexpr static std::size_t averageFilter = 5;

        foc::MotorDriver& driver;
        foc::Encoder& encoder;
        foc::Volts vdc;
        [[no_unique_address]] foc::ClarkePark transforms;
        ResistanceAndInductanceConfig resistanceAndInductanceConfig;
        PolePairsConfig polePairsConfig;
        infra::BoundedDeque<float>::WithMaxSize<averageFilter> currentSamples;
        infra::BoundedVector<float>::WithMaxSize<inductanceSamplesSize - averageFilter> filteredCurrentSample;
        std::size_t currentSampleIndex{ 0 };
        foc::Radians initialPosition{ 0.0f };
        foc::Radians previousPosition{ 0.0f };
        float accumulatedRotation{ 0.0f };
        infra::AutoResetFunction<void(std::optional<foc::Ohm>, std::optional<foc::MilliHenry>)> onResistanceAndInductanceDone;
        infra::AutoResetFunction<void(std::optional<std::size_t>)> onPolePairsDone;

        infra::TimerSingleShot settleTimer;
    };
}
