#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include <QObject>
#include <cstdint>

namespace simulator
{
    class OnlineMechanicalRls
        : public QObject
        , public foc::ThreePhaseMotorModelObserver
    {
        Q_OBJECT

    public:
        OnlineMechanicalRls(foc::ThreePhaseMotorModel& model,
            uint8_t polePairs,
            foc::NewtonMeter torqueConstant,
            hal::Hertz baseFrequency,
            QObject* parent = nullptr);

        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians thetaMech, foc::RadiansPerSecond omegaMech) override;
        void StatorVoltages(foc::ThreePhase phaseVoltages, foc::TwoPhase alphaBeta) override;
        void Finished() override;

    signals:
        void mechanicalEstimatesChanged(float Bhat, float Jhat);

    private:
        static constexpr uint32_t estimatorWindowFrequencyHz{ 1000 };

        uint8_t polePairs;
        [[no_unique_address]] foc::Clarke clarke;
        [[no_unique_address]] foc::Park park;
        services::RealTimeFrictionAndInertiaEstimator estimator;
        uint32_t samplesPerWindow;
        uint32_t samplesInWindow{ 0 };
        float sumIq{ 0.0f };
        float sumSpeed{ 0.0f };
    };
}
