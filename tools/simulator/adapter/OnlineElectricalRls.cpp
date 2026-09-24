#include "tools/simulator/adapter/OnlineElectricalRls.hpp"
#include "core/foc/math/FastTrigonometry.hpp"

namespace simulator
{
    OnlineElectricalRls::OnlineElectricalRls(foc::ThreePhaseMotorModel& model, uint8_t polePairs, hal::Hertz baseFrequency, QObject* parent)
        : QObject(parent)
        , foc::ThreePhaseMotorModelObserver(model)
        , polePairs(polePairs)
        , estimator(services::RealTimeResistanceAndInductanceEstimator::defaultForgettingFactor, hal::Hertz{ estimatorWindowFrequencyHz })
        , samplesPerWindow(baseFrequency.Value() / estimatorWindowFrequencyHz)
    {}

    void OnlineElectricalRls::Started()
    {
        samplesInWindow = 0;
        sumVd = 0.0f;
        sumId = 0.0f;
        sumSpeedTimesIq = 0.0f;
    }

    void OnlineElectricalRls::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians thetaMech, foc::RadiansPerSecond omegaMech)
    {
        const float thetaElec = static_cast<float>(polePairs) * thetaMech.Value();
        const float cosTheta = foc::FastTrigonometry::Cosine(thetaElec);
        const float sinTheta = foc::FastTrigonometry::Sine(thetaElec);

        const foc::TwoPhase iAlphaBeta = clarke.Forward({ currents.a.Value(), currents.b.Value(), currents.c.Value() });
        const foc::RotatingFrame idq = park.Forward(iAlphaBeta, cosTheta, sinTheta);
        const foc::RotatingFrame vdq = park.Forward(lastVAlphaBeta, cosTheta, sinTheta);

        sumVd += vdq.d;
        sumId += idq.d;
        sumSpeedTimesIq += static_cast<float>(polePairs) * omegaMech.Value() * idq.q;

        if (++samplesInWindow < samplesPerWindow)
            return;

        const float count = static_cast<float>(samplesInWindow);
        estimator.Update(foc::ElectricalWindow{ foc::Volts{ sumVd / count }, foc::Ampere{ sumId / count }, foc::Ampere{ idq.d }, sumSpeedTimesIq / count });
        Started();

        emit electricalEstimatesChanged(estimator.CurrentResistance().Value(),
            estimator.CurrentInductance().Value() * 0.001f);
    }

    void OnlineElectricalRls::StatorVoltages(foc::ThreePhase /*phaseVoltages*/, foc::TwoPhase alphaBeta)
    {
        lastVAlphaBeta = alphaBeta;
    }

    void OnlineElectricalRls::Finished()
    {}
}
