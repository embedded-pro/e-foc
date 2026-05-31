#include "tools/simulator/view/gui/OnlineElectricalRls.hpp"
#include "core/foc/implementations/TrigonometricImpl.hpp"

namespace simulator
{
    OnlineElectricalRls::OnlineElectricalRls(ThreePhaseMotorModel& model, uint8_t polePairs, hal::Hertz baseFrequency, QObject* parent)
        : QObject(parent)
        , ThreePhaseMotorModelObserver(model)
        , polePairs(polePairs)
        , estimator(services::RealTimeResistanceAndInductanceEstimator::defaultForgettingFactor, baseFrequency)
    {}

    void OnlineElectricalRls::Started()
    {}

    void OnlineElectricalRls::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians thetaMech, foc::RadiansPerSecond omegaMech)
    {
        const float thetaElec = static_cast<float>(polePairs) * thetaMech.Value();
        const float cosTheta = foc::FastTrigonometry::Cosine(thetaElec);
        const float sinTheta = foc::FastTrigonometry::Sine(thetaElec);

        const foc::TwoPhase iAlphaBeta = clarke.Forward({ currents.a.Value(), currents.b.Value(), currents.c.Value() });
        const foc::RotatingFrame idq = park.Forward(iAlphaBeta, cosTheta, sinTheta);
        const foc::RotatingFrame vdq = park.Forward(lastVAlphaBeta, cosTheta, sinTheta);

        estimator.Update(foc::Volts{ vdq.d }, foc::Ampere{ idq.d }, foc::Ampere{ idq.q },
            foc::RadiansPerSecond{ static_cast<float>(polePairs) * omegaMech.Value() });

        // ScopesPanel scales Lhat by 1000 to display millihenries; emit henries.
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
