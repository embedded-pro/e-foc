#include "tools/simulator/view/gui/OnlineMechanicalRls.hpp"

namespace simulator
{
    OnlineMechanicalRls::OnlineMechanicalRls(ThreePhaseMotorModel& model,
        uint8_t polePairs,
        foc::NewtonMeter torqueConstant,
        hal::Hertz baseFrequency,
        QObject* parent)
        : QObject(parent)
        , ThreePhaseMotorModelObserver(model)
        , polePairs(polePairs)
        , estimator(services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, baseFrequency)
    {
        estimator.SetTorqueConstant(torqueConstant);
    }

    void OnlineMechanicalRls::Started()
    {}

    void OnlineMechanicalRls::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians thetaMech, foc::RadiansPerSecond omegaMech)
    {
        estimator.Update(currents, omegaMech, foc::Radians{ static_cast<float>(polePairs) * thetaMech.Value() });
        emit mechanicalEstimatesChanged(estimator.CurrentFriction().Value(), estimator.CurrentInertia().Value());
    }

    void OnlineMechanicalRls::StatorVoltages(foc::ThreePhase /*phaseVoltages*/, foc::TwoPhase /*alphaBeta*/)
    {}

    void OnlineMechanicalRls::Finished()
    {}
}
