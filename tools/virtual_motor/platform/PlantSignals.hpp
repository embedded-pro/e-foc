#pragma once

#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "tools/virtual_motor/port/PlantPortMessages.hpp"
#include <array>
#include <cstdint>

namespace virtual_motor
{
    enum class Signal : uint8_t
    {
        currentA,
        currentB,
        currentC,
        measuredCurrentA,
        measuredCurrentB,
        measuredCurrentC,
        dutyA,
        dutyB,
        dutyC,
        legVoltageA,
        legVoltageB,
        legVoltageC,
        statorVoltageAlpha,
        statorVoltageBeta,
        currentD,
        currentQ,
        rotorAngle,
        encoderAngle,
        rotorSpeed,
        electricalTorque,
        loadTorque,
        externalTorque,
        busVoltage,
        busCurrent,
        windingTemperature,
    };

    using SignalSample = std::array<float, port::signalCount>;

    class PlantSignals
        : public foc::ThreePhaseMotorModelObserver
    {
    public:
        PlantSignals(foc::ThreePhaseMotorModel& model, uint8_t polePairs);

        SignalSample Sample(const foc::PhasePwmDutyCycles& appliedDuties, foc::NewtonMeter loadTorque);

        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta, foc::RadiansPerSecond omegaMech) override;
        void StatorVoltages(foc::ThreePhase phaseVoltages, foc::TwoPhase alphaBeta) override;

    private:
        void SampleElectrical(SignalSample& sample, const foc::PhasePwmDutyCycles& appliedDuties) const;
        void SampleMechanical(SignalSample& sample, foc::NewtonMeter loadTorque);

        foc::ThreePhaseMotorModel& model;
        uint8_t polePairs;
        foc::ThreePhase legVoltages{};
        foc::TwoPhase statorVoltage{};
    };
}
