#pragma once

#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/BoundedDeque.hpp"
#include "infra/util/BoundedVector.hpp"
#include "source/foc/interfaces/Controller.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/services/parameter_identification/MechanicalParametersIdentification.hpp"

namespace services
{
    class MechanicalParametersIdentificationImpl
        : public MechanicalParametersIdentification
    {
    public:
        MechanicalParametersIdentificationImpl(foc::SpeedController& controller, foc::MotorDriver& driver, foc::Encoder& encoder);

        void EstimateFriction(const FrictionConfig& config, const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>)>& onDone) override;
        void EstimateInertia(const InertiaConfig& config, foc::NewtonMeterSecondPerRadian damping, const infra::Function<void(std::optional<foc::NewtonMeterSecondSquared>)>& onDone) override;

    private:
        void CalculateFriction();
        void CollectInertiaSamples();
        void CalculateInertia();

        static constexpr std::size_t rawSampleFilterSize = 16;
        static constexpr std::size_t maxFilteredSamples = 200;
        static constexpr std::size_t maxInertiaSamples = 500;

        foc::SpeedController& controller;
        foc::MotorDriver& driver;
        foc::Encoder& encoder;
        float samplingPeriod;
        FrictionConfig frictionConfig;
        InertiaConfig inertiaConfig;
        foc::NewtonMeterSecondPerRadian currentDamping{ 0.0f };
        float previousPosition{ 0.0f };
        infra::BoundedDeque<float>::WithMaxSize<rawSampleFilterSize> rawCurrentSamples;
        infra::BoundedVector<float>::WithMaxSize<maxFilteredSamples> filteredCurrentSamples;
        infra::BoundedVector<float>::WithMaxSize<maxInertiaSamples> speedSamples;
        infra::BoundedVector<float>::WithMaxSize<maxInertiaSamples> timeSamples;
        std::size_t sampleIndex{ 0 };
        infra::AutoResetFunction<void(std::optional<foc::NewtonMeterSecondPerRadian>)> onFrictionDone;
        infra::AutoResetFunction<void(std::optional<foc::NewtonMeterSecondSquared>)> onInertiaDone;

        infra::TimerSingleShot settleTimer;
    };
}
