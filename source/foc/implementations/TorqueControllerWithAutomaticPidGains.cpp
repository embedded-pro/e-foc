#include "source/foc/implementations/TorqueControllerWithAutomaticPidGains.hpp"

namespace
{
    float CurrentLoopBandwidth(hal::Hertz baseFrequency, float nyquistFactor)
    {
        return (static_cast<float>(baseFrequency.Value()) / nyquistFactor) * 2.0f * std::numbers::pi_v<float>;
    }
}

namespace foc
{
    TorqueControllerWithAutomaticPidGains::TorqueControllerWithAutomaticPidGains(MotorDriver& interface, Encoder& position, FieldOrientedControllerTorqueControl& foc)
        : TorqueControllerImpl(interface, position, foc)
    {
    }

    void TorqueControllerWithAutomaticPidGains::SetPidBasedOnResistanceAndInductance(Volts Vdc, Ohm resistance, MilliHenry inductance, float nyquistFactor)
    {
        auto wc = CurrentLoopBandwidth(BaseFrequency(), nyquistFactor);
        auto kp = static_cast<float>(inductance.Value()) * 0.001f * wc;
        auto ki = static_cast<float>(resistance.Value()) * wc;

        auto dPid = controllers::PidTunings<float>{ kp, ki, 0.0f };
        auto qPid = controllers::PidTunings<float>{ kp, ki, 0.0f };

        SetCurrentTunings(Vdc, foc::IdAndIqTunings{ dPid, qPid });
    }
}
