#pragma once

#ifndef Q_MOC_RUN
#include "core/foc/implementations/TransformsClarkePark.hpp"
#include "core/foc/implementations/TrigonometricImpl.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "numerical/estimators/online/RecursiveLeastSquares.hpp"
#include "tools/simulator/model/Model.hpp"
#include "tools/simulator/services/OnlineElectricalRls.hpp"
#endif

#include <QObject>
#include <cstddef>
#include <cstdint>

namespace simulator
{
    class OnlineMechanicalRls
        : public QObject
        , public ThreePhaseMotorModelObserver
    {
        Q_OBJECT

    public:
        OnlineMechanicalRls(ThreePhaseMotorModel& model,
            const OnlineElectricalRls& electricalRls,
            const ThreePhaseMotorModel::Parameters& parameters,
            hal::Hertz baseFrequency,
            std::size_t decimation,
            QObject* parent = nullptr);

        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians thetaMech, foc::RadiansPerSecond omegaMech) override;
        void StatorVoltages(foc::ThreePhase phaseVoltages, foc::TwoPhase alphaBeta) override;
        void Finished() override;

        float FrictionEstimate() const
        {
            return bHat;
        }

        float InertiaEstimate() const
        {
            return jHat;
        }

        void UpdateForTest(float te, float omega, float dt);

    signals:
        void mechanicalEstimatesChanged(float Bhat, float Jhat);

    private:
        void Update(float te, float omega);

        const OnlineElectricalRls& electricalRls;
        ThreePhaseMotorModel::Parameters parameters;
        std::size_t decimation;
        std::size_t counter{ 0 };
        float tsm;
        foc::Clarke clarke;
        foc::Park park;
        foc::FastTrigonometry trig;
        estimators::RecursiveLeastSquares<float, 2> rls;
        bool havePrev{ false };
        float omegaPrev{ 0.0f };
        float bHat{ 0.0f };
        float jHat{ 0.0f };
    };
}
