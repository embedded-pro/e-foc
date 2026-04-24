#include "tools/simulator/services/OnlineElectricalRls.hpp"
#include <cmath>

namespace simulator
{
    OnlineElectricalRls::OnlineElectricalRls(ThreePhaseMotorModel& model, uint8_t polePairs, hal::Hertz baseFrequency, QObject* parent)
        : QObject(parent)
        , ThreePhaseMotorModelObserver(model)
        , polePairs(polePairs)
        , ts(1.0f / static_cast<float>(baseFrequency.Value()))
        , rls(1000.0f, 0.995f)
    {
    }

    void OnlineElectricalRls::Started()
    {
        haveVoltage = false;
        haveCurrentPrev = false;
        idPrev = 0.0f;
        rHat = 0.0f;
        lHat = 0.001f;
    }

    void OnlineElectricalRls::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians thetaMech, foc::RadiansPerSecond omegaMech)
    {
        if (!haveVoltage)
            return;

        const float thetaElec = static_cast<float>(polePairs) * thetaMech.Value();
        const float cosTheta = trig.Cosine(thetaElec);
        const float sinTheta = trig.Sine(thetaElec);

        const foc::ThreePhase threePhase{ currents.a.Value(), currents.b.Value(), currents.c.Value() };
        const foc::TwoPhase iAlphaBeta = clarke.Forward(threePhase);
        const foc::RotatingFrame idq = park.Forward(iAlphaBeta, cosTheta, sinTheta);

        const foc::TwoPhase vAlphaBeta = lastVAlphaBeta;
        const foc::RotatingFrame vdq = park.Forward(vAlphaBeta, cosTheta, sinTheta);

        const float omegaElec = static_cast<float>(polePairs) * omegaMech.Value();
        Update(vdq.d, idq.d, idq.q, omegaElec);
    }

    void OnlineElectricalRls::StatorVoltages(foc::ThreePhase /*phaseVoltages*/, foc::TwoPhase alphaBeta)
    {
        lastVAlphaBeta = alphaBeta;
        haveVoltage = true;
    }

    void OnlineElectricalRls::Finished()
    {
    }

    void OnlineElectricalRls::UpdateForTest(float vd, float id, float iq, float omegaElec)
    {
        Update(vd, id, iq, omegaElec);
    }

    void OnlineElectricalRls::Update(float vd, float id, float iq, float omegaElec)
    {
        if (!haveCurrentPrev)
        {
            idPrev = id;
            haveCurrentPrev = true;
            return;
        }

        math::Matrix<float, 2, 1> phi;
        phi.at(0, 0) = id;
        phi.at(1, 0) = (id - idPrev) / ts;

        math::Matrix<float, 1, 1> y;
        y.at(0, 0) = vd + omegaElec * lHat * iq;

        rls.Update(phi, y);

        rHat = rls.Coefficients().at(0, 0);
        lHat = rls.Coefficients().at(1, 0);

        idPrev = id;

        emit electricalEstimatesChanged(rHat, lHat);
    }
}
