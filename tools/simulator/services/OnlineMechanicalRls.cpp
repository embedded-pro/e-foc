#include "tools/simulator/services/OnlineMechanicalRls.hpp"
#include <cmath>

namespace simulator
{
    OnlineMechanicalRls::OnlineMechanicalRls(ThreePhaseMotorModel& model,
        const OnlineElectricalRls& electricalRls,
        const ThreePhaseMotorModel::Parameters& parameters,
        hal::Hertz baseFrequency,
        std::size_t decimation,
        QObject* parent)
        : QObject(parent)
        , ThreePhaseMotorModelObserver(model)
        , electricalRls(electricalRls)
        , parameters(parameters)
        , decimation(decimation)
        , tsm(static_cast<float>(decimation) / static_cast<float>(baseFrequency.Value()))
        , rls(1000.0f, 0.995f)
    {
    }

    void OnlineMechanicalRls::Started()
    {
        counter = 0;
        havePrev = false;
        omegaPrev = 0.0f;
        bHat = 0.0f;
        jHat = 0.0f;
    }

    void OnlineMechanicalRls::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians thetaMech, foc::RadiansPerSecond omegaMech)
    {
        ++counter;
        if (counter < decimation)
            return;
        counter = 0;

        const float thetaElec = static_cast<float>(parameters.p) * thetaMech.Value();
        const float cosTheta = trig.Cosine(thetaElec);
        const float sinTheta = trig.Sine(thetaElec);

        const foc::ThreePhase threePhase{ currents.a.Value(), currents.b.Value(), currents.c.Value() };
        const foc::TwoPhase iAlphaBeta = clarke.Forward(threePhase);
        const foc::RotatingFrame idq = park.Forward(iAlphaBeta, cosTheta, sinTheta);

        const float psiF = parameters.psi_f.Value();
        const float Te = 1.5f * static_cast<float>(parameters.p) * psiF * idq.q;
        const float omega = omegaMech.Value();

        Update(Te, omega);
    }

    void OnlineMechanicalRls::StatorVoltages(foc::ThreePhase /*phaseVoltages*/, foc::TwoPhase /*alphaBeta*/)
    {
    }

    void OnlineMechanicalRls::Finished()
    {
    }

    void OnlineMechanicalRls::UpdateForTest(float te, float omega, float /*dt*/)
    {
        Update(te, omega);
    }

    void OnlineMechanicalRls::Update(float te, float omega)
    {
        if (!havePrev)
        {
            omegaPrev = omega;
            havePrev = true;
            return;
        }

        math::Matrix<float, 2, 1> phi;
        phi.at(0, 0) = (omega - omegaPrev) / tsm;
        phi.at(1, 0) = omega;

        math::Matrix<float, 1, 1> y;
        y.at(0, 0) = te;

        rls.Update(phi, y);

        jHat = rls.Coefficients().at(0, 0);
        bHat = rls.Coefficients().at(1, 0);

        omegaPrev = omega;

        emit mechanicalEstimatesChanged(bHat, jHat);
    }
}
