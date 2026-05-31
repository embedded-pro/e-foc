#pragma once

#ifndef Q_MOC_RUN
#include "core/foc/implementations/TransformsClarkePark.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include "tools/simulator/model/Model.hpp"
#endif

#include <QObject>
#include <cstdint>

namespace simulator
{
    // Thin Qt observer that adapts the simulator's three-phase model samples
    // to the production RealTimeResistanceAndInductanceEstimator and re-emits
    // the resulting estimates as Qt signals for the GUI scopes.
    class OnlineElectricalRls
        : public QObject
        , public ThreePhaseMotorModelObserver
    {
        Q_OBJECT

    public:
        OnlineElectricalRls(ThreePhaseMotorModel& model, uint8_t polePairs, hal::Hertz baseFrequency, QObject* parent = nullptr);

        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians thetaMech, foc::RadiansPerSecond omegaMech) override;
        void StatorVoltages(foc::ThreePhase phaseVoltages, foc::TwoPhase alphaBeta) override;
        void Finished() override;

    signals:
        void electricalEstimatesChanged(float Rhat, float Lhat);

    private:
        uint8_t polePairs;
        [[no_unique_address]] foc::Clarke clarke;
        [[no_unique_address]] foc::Park park;
        services::RealTimeResistanceAndInductanceEstimator estimator;
        foc::TwoPhase lastVAlphaBeta{};
    };
}
