#include "tools/simulator/adapter/OnlineMechanicalRls.hpp"
#include "numerical/math/AngleUtilities.hpp"
#include "numerical/math/FastTrigonometry.hpp"

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
        , window(samplesPerWindow)
    {
        estimator.SetTorqueConstant(torqueConstant);
    }

    void OnlineMechanicalRls::Started()
    {
        samplesInWindow = 0;
        window.Restart();
        primed = false;
    }

    void OnlineMechanicalRls::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians thetaMech, foc::RadiansPerSecond)
    {
        const float thetaElec = static_cast<float>(polePairs) * thetaMech.Value();
        const auto idq = park.Forward(clarke.Forward({ currents.a.Value(), currents.b.Value(), currents.c.Value() }), math::FastCosine(thetaElec), math::FastSine(thetaElec));
        window.Accumulate(idq.q, primed ? math::WrapToPi(thetaMech.Value() - previousAngle) : 0.0f);
        previousAngle = thetaMech.Value();
        primed = true;

        if (++samplesInWindow < samplesPerWindow)
            return;

        samplesInWindow = 0;
        estimator.Update(window.Close());
        emit mechanicalEstimatesChanged(estimator.CurrentFriction().Value(), estimator.CurrentInertia().Value());
    }

    void OnlineMechanicalRls::StatorVoltages(foc::ThreePhase, foc::TwoPhase)
    {}

    void OnlineMechanicalRls::Finished()
    {}
}
