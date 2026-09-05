#pragma once

#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "core/services/electrical_system_ident/ResistanceEstimator.hpp"
#include "core/services/electrical_system_ident/SinusoidalInductanceEstimator.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include <numbers>

namespace services
{
    class ElectricalParametersIdentificationImpl
        : public ElectricalParametersIdentification
    {
    public:
        ElectricalParametersIdentificationImpl(drivers::ThreePhaseInverter& driver, drivers::Encoder& encoder, foc::Volts vdc);

        void EstimateResistanceAndInductance(const ResistanceAndInductanceConfig& config, const infra::Function<void(ResistanceInductanceResult)>& onDone) override;
        void EstimateNumberOfPolePairs(const PolePairsConfig& config, const infra::Function<void(std::optional<std::size_t>)>& onDone) override;

    private:
        void OnResistanceDone(ResistanceEstimator::Result result);
        void RunPolePairLogic();
        void ApplyNextElectricalAngle();
        void CalculatePolePairs();
        void AbortPolePairs();

        static constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
        static constexpr std::size_t stepsPerRevolution = 12;
        static constexpr float minRotationThreshold = std::numbers::pi_v<float> / 2.0f;

        drivers::ThreePhaseInverter& driver;
        drivers::Encoder& encoder;
        foc::Volts vdc;
        [[no_unique_address]] foc::ClarkePark transforms;

        ResistanceEstimator resistanceEstimator;
        SinusoidalInductanceEstimator inductanceEstimator;

        ResistanceAndInductanceConfig rlConfig;
        PolePairsConfig polePairsConfig;

        ResistanceInductanceResult pendingResult;
        infra::AutoResetFunction<void(ResistanceInductanceResult)> onResistanceAndInductanceDone;
        infra::AutoResetFunction<void(std::optional<std::size_t>)> onPolePairsDone;

        foc::Radians initialPosition{ 0.0f };
        foc::Radians previousPosition{ 0.0f };
        float accumulatedRotation{ 0.0f };
        std::size_t currentSampleIndex{ 0 };

        bool rlRunning{ false };
        bool polePairsRunning{ false };
        infra::TimerSingleShot settleTimer;
    };
}
