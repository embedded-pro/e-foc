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
        , estimator(services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, hal::Hertz{ estimatorWindowFrequencyHz })
        , samplesPerWindow(baseFrequency.Value() / estimatorWindowFrequencyHz)
    {
        estimator.SetTorqueConstant(torqueConstant);
    }

    void OnlineMechanicalRls::Started()
    {
        samplesInWindow = 0;
        sumIq = 0.0f;
        sumSpeed = 0.0f;
    }

    void OnlineMechanicalRls::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians thetaMech, foc::RadiansPerSecond omegaMech)
    {
        const float thetaElec = static_cast<float>(polePairs) * thetaMech.Value();
        const auto idq = park.Forward(clarke.Forward({ currents.a.Value(), currents.b.Value(), currents.c.Value() }), foc::FastTrigonometry::Cosine(thetaElec), foc::FastTrigonometry::Sine(thetaElec));
        sumIq += idq.q;
        sumSpeed += omegaMech.Value();

        if (++samplesInWindow < samplesPerWindow)
            return;

        const float count = static_cast<float>(samplesInWindow);
        estimator.Update(foc::MechanicalWindow{ foc::Ampere{ sumIq / count }, foc::RadiansPerSecond{ sumSpeed / count } });
        Started();
        emit mechanicalEstimatesChanged(estimator.CurrentFriction().Value(), estimator.CurrentInertia().Value());
    }

    void OnlineMechanicalRls::StatorVoltages(foc::ThreePhase /*phaseVoltages*/, foc::TwoPhase /*alphaBeta*/)
    {}

    void OnlineMechanicalRls::Finished()
    {}
}
