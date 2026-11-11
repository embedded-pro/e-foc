#include "tools/virtual_motor/platform/PlantSignals.hpp"

namespace virtual_motor
{
    namespace
    {
        constexpr float torqueConstant{ 1.5f };

        void Set(SignalSample& sample, Signal signal, float value)
        {
            sample[static_cast<std::size_t>(signal)] = value;
        }
    }

    PlantSignals::PlantSignals(foc::ThreePhaseMotorModel& model, uint8_t polePairs)
        : foc::ThreePhaseMotorModelObserver(model)
        , model(model)
        , polePairs(polePairs)
    {}

    SignalSample PlantSignals::Sample(const foc::PhasePwmDutyCycles& appliedDuties, foc::NewtonMeter loadTorque)
    {
        SignalSample sample{};
        SampleElectrical(sample, appliedDuties);
        SampleMechanical(sample, loadTorque);
        return sample;
    }

    void PlantSignals::Started()
    {}

    void PlantSignals::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents, foc::Radians, foc::RadiansPerSecond)
    {}

    void PlantSignals::StatorVoltages(foc::ThreePhase phaseVoltages, foc::TwoPhase alphaBeta)
    {
        legVoltages = phaseVoltages;
        statorVoltage = alphaBeta;
    }

    void PlantSignals::SampleElectrical(SignalSample& sample, const foc::PhasePwmDutyCycles& appliedDuties) const
    {
        const auto currents = model.Electrical().Currents();
        const auto measured = model.CurrentSensor().Last();
        Set(sample, Signal::currentA, currents.a.Value());
        Set(sample, Signal::currentB, currents.b.Value());
        Set(sample, Signal::currentC, currents.c.Value());
        Set(sample, Signal::measuredCurrentA, measured.a.Value());
        Set(sample, Signal::measuredCurrentB, measured.b.Value());
        Set(sample, Signal::measuredCurrentC, measured.c.Value());
        Set(sample, Signal::dutyA, foc::DutyFraction(appliedDuties.a));
        Set(sample, Signal::dutyB, foc::DutyFraction(appliedDuties.b));
        Set(sample, Signal::dutyC, foc::DutyFraction(appliedDuties.c));
        Set(sample, Signal::legVoltageA, legVoltages.a);
        Set(sample, Signal::legVoltageB, legVoltages.b);
        Set(sample, Signal::legVoltageC, legVoltages.c);
        Set(sample, Signal::statorVoltageAlpha, statorVoltage.alpha);
        Set(sample, Signal::statorVoltageBeta, statorVoltage.beta);
    }

    void PlantSignals::SampleMechanical(SignalSample& sample, foc::NewtonMeter loadTorque)
    {
        const auto dq = model.Electrical().LastDqCurrents();
        const auto winding = model.Winding();
        const auto& mechanics = model.Mechanics();
        Set(sample, Signal::currentD, dq.d);
        Set(sample, Signal::currentQ, dq.q);
        Set(sample, Signal::rotorAngle, mechanics.Angle().Value());
        Set(sample, Signal::encoderAngle, model.Encoder().Read().Value());
        Set(sample, Signal::rotorSpeed, mechanics.Speed().Value());
        Set(sample, Signal::electricalTorque, torqueConstant * polePairs * (winding.fluxLinkage * dq.q + (winding.inductanceD - winding.inductanceQ) * dq.d * dq.q));
        Set(sample, Signal::loadTorque, loadTorque.Value());
        Set(sample, Signal::externalTorque, mechanics.ExternalTorque().Value());
        Set(sample, Signal::busVoltage, model.Supply().Voltage().Value());
        Set(sample, Signal::busCurrent, model.Supply().Current().Value());
        Set(sample, Signal::windingTemperature, model.Thermal().Temperature());
    }
}
