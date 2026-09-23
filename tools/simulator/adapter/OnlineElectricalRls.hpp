#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include <QObject>
#include <cstdint>

namespace simulator
{
    class OnlineElectricalRls
        : public QObject
        , public foc::ThreePhaseMotorModelObserver
    {
        Q_OBJECT

    public:
        OnlineElectricalRls(foc::ThreePhaseMotorModel& model, uint8_t polePairs, hal::Hertz baseFrequency, QObject* parent = nullptr);

        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians thetaMech, foc::RadiansPerSecond omegaMech) override;
        void StatorVoltages(foc::ThreePhase phaseVoltages, foc::TwoPhase alphaBeta) override;
        void Finished() override;

    signals:
        void electricalEstimatesChanged(float Rhat, float Lhat);

    private:
        static constexpr uint32_t estimatorWindowFrequencyHz{ 1000 };

        uint8_t polePairs;
        [[no_unique_address]] foc::Clarke clarke;
        [[no_unique_address]] foc::Park park;
        services::RealTimeResistanceAndInductanceEstimator estimator;
        foc::TwoPhase lastVAlphaBeta{};
        uint32_t samplesPerWindow;
        uint32_t samplesInWindow{ 0 };
        float sumVd{ 0.0f };
        float sumId{ 0.0f };
        float sumSpeedTimesIq{ 0.0f };
    };
}
