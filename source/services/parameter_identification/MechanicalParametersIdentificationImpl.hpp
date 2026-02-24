#pragma once

#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "numerical/estimators/RecursiveLeastSquares.hpp"
#include "source/foc/implementations/TransformsClarkePark.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/foc/interfaces/Foc.hpp"
#include "source/services/parameter_identification/MechanicalParametersIdentification.hpp"

namespace services
{
    class MechanicalParametersIdentificationImpl
        : public MechanicalParametersIdentification
    {
    public:
        MechanicalParametersIdentificationImpl(foc::FocSpeed& controller, foc::MotorDriver& driver, foc::Encoder& encoder);

        void EstimateFrictionAndInertia(const foc::NewtonMeter& torqueConstant, std::size_t numberOfPolePairs, const Config& config, const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)>& onDone) override;

    private:
        void OnSamplingUpdate(foc::PhaseCurrents currentPhases, const foc::NewtonMeter& torqueConstant);

        foc::FocSpeed& controller;
        foc::MotorDriver& driver;
        foc::Encoder& encoder;

        float samplingPeriod;
        Config currentConfig;
        infra::AutoResetFunction<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)> onDone;

        using MotorRLS = estimators::RecursiveLeastSquares<float, 3>;
        std::optional<MotorRLS> rls;

        float previousPosition{ 0.0f };
        float previousSpeed{ 0.0f };
        float polePairs{ 1.0f };
        [[no_unique_address]] foc::ClarkePark transform;
        infra::TimerSingleShot timeoutTimer;
        MotorRLS::InputMatrix regressor;
        math::Matrix<float, 1, 1> torque;
    };
}
