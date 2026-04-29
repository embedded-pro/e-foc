#pragma once

#ifndef Q_MOC_RUN
#include "core/foc/interfaces/Units.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "tools/simulator/model/Model.hpp"
#endif

#include <QObject>
#include <cstdint>

namespace simulator
{
    // Thin Qt observer that adapts the simulator's three-phase model samples
    // to the production RealTimeFrictionAndInertiaEstimator and re-emits the
    // resulting estimates as Qt signals for the GUI scopes.
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
