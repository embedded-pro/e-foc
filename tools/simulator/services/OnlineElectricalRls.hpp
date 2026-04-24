#pragma once

#ifndef Q_MOC_RUN
#include "core/foc/implementations/TransformsClarkePark.hpp"
#include "core/foc/implementations/TrigonometricImpl.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "numerical/estimators/online/RecursiveLeastSquares.hpp"
#include "tools/simulator/model/Model.hpp"
#endif

#include <QObject>
#include <cstdint>

namespace simulator
{
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

        float ResistanceEstimate() const
        {
            return rHat;
        }

        float InductanceEstimate() const
        {
            return lHat;
        }

        void UpdateForTest(float vd, float id, float iq, float omegaElec);

    signals:
        void electricalEstimatesChanged(float Rhat, float Lhat);

    private:
        void Update(float vd, float id, float iq, float omegaElec);

        uint8_t polePairs;
        float ts;
        foc::Clarke clarke;
        foc::Park park;
        foc::FastTrigonometry trig;
        estimators::RecursiveLeastSquares<float, 2> rls;
        bool haveVoltage{ false };
        foc::TwoPhase lastVAlphaBeta{};
        float idPrev{ 0.0f };
        bool haveCurrentPrev{ false };
        float rHat{ 0.0f };
        float lHat{ 0.001f };
    };
}
