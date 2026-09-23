#include "tools/simulator/adapter/OnlineMechanicalRls.hpp"
#include "core/foc/math/FastTrigonometry.hpp"

namespace simulator
{
    OnlineMechanicalRls::OnlineMechanicalRls(foc::ThreePhaseMotorModel& model,
        uint8_t polePairs,
        foc::NewtonMeter torqueConstant,
        hal::Hertz baseFrequency,
        QObject* parent)
        : QObject(parent)
        , foc::ThreePhaseMotorModelObserver(model)
        , polePairs(polePairs)
        , estimator(services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, baseFrequency)
    {
        estimator.SetTorqueConstant(torqueConstant);
    }

    void OnlineMechanicalRls::Started()
    {}

    void OnlineMechanicalRls::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians thetaMech, foc::RadiansPerSecond omegaMech)
    {
        const float thetaElec = static_cast<float>(polePairs) * thetaMech.Value();
        const auto idq = park.Forward(clarke.Forward({ currents.a.Value(), currents.b.Value(), currents.c.Value() }), foc::FastTrigonometry::Cosine(thetaElec), foc::FastTrigonometry::Sine(thetaElec));
        estimator.Update(foc::MechanicalWindow{ foc::Ampere{ idq.q }, omegaMech });
        emit mechanicalEstimatesChanged(estimator.CurrentFriction().Value(), estimator.CurrentInertia().Value());
    }

    void OnlineMechanicalRls::StatorVoltages(foc::ThreePhase /*phaseVoltages*/, foc::TwoPhase /*alphaBeta*/)
    {}

    void OnlineMechanicalRls::Finished()
    {}
}
