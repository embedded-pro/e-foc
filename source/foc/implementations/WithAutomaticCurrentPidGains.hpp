#pragma once

#include "source/foc/interfaces/Foc.hpp"
#include <numbers>

namespace foc
{
    class WithAutomaticCurrentPidGains
    {
    public:
        explicit WithAutomaticCurrentPidGains(FocBase& foc)
            : foc{ foc }
        {}

        void SetPidBasedOnResistanceAndInductance(Volts Vdc, Ohm resistance, MilliHenry inductance, hal::Hertz baseFrequency, float nyquistFactor)
        {
            auto wc = CurrentLoopBandwidth(baseFrequency, nyquistFactor);
            auto kp = inductance.Value() * 0.001f * wc;
            auto ki = resistance.Value() * wc;

            auto dPid = controllers::PidTunings<float>{ kp, ki, 0.0f };
            auto qPid = controllers::PidTunings<float>{ kp, ki, 0.0f };

            foc.SetCurrentTunings(Vdc, IdAndIqTunings{ dPid, qPid });
        }

    private:
        static float CurrentLoopBandwidth(hal::Hertz baseFrequency, float nyquistFactor)
        {
            return (static_cast<float>(baseFrequency.Value()) / nyquistFactor) * 2.0f * std::numbers::pi_v<float>;
        }

    private:
        FocBase& foc;
    };
}
