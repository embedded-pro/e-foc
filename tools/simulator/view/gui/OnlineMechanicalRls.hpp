#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include <QObject>
#include <cstdint>

using namespace foc;

namespace simulator
{
    class OnlineMechanicalRls
        : public QObject
        , public ThreePhaseMotorModelObserver
    {
        Q_OBJECT

    public:
        OnlineMechanicalRls(ThreePhaseMotorModel& model,
            uint8_t polePairs,
            foc::NewtonMeter torqueConstant,
            hal::Hertz baseFrequency,
            QObject* parent = nullptr);

        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians thetaMech, foc::RadiansPerSecond omegaMech) override;
        void StatorVoltages(foc::ThreePhase phaseVoltages, foc::TwoPhase alphaBeta) override;
        void Finished() override;

    signals:
        void mechanicalEstimatesChanged(float Bhat, float Jhat);

    private:
        uint8_t polePairs;
        services::RealTimeFrictionAndInertiaEstimator estimator;
    };
}
