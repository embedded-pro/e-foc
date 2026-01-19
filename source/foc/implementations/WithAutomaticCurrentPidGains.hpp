#pragma once

#include "foc/interfaces/Units.hpp"
#include "source/foc/interfaces/FieldOrientedController.hpp"
#include <numbers>

namespace foc
{
    template<typename TController>
    class WithAutomaticCurrentPidGains : public TController
    {
    public:
        using TController::TController;

        void SetPidBasedOnResistanceAndInductance(Volts Vdc, Ohm resistance, MilliHenry inductance, float nyquistFactor)
        {
            auto wc = CurrentLoopBandwidth(this->BaseFrequency(), nyquistFactor);
            auto kp = static_cast<float>(inductance.Value()) * 0.001f * wc;
            auto ki = static_cast<float>(resistance.Value()) * wc;

            auto dPid = controllers::PidTunings<float>{ kp, ki, 0.0f };
            auto qPid = controllers::PidTunings<float>{ kp, ki, 0.0f };

            this->SetCurrentTunings(Vdc, IdAndIqTunings{ dPid, qPid });
        }

    private:
        static float CurrentLoopBandwidth(hal::Hertz baseFrequency, float nyquistFactor)
        {
            return (static_cast<float>(baseFrequency.Value()) / nyquistFactor) * 2.0f * std::numbers::pi_v<float>;
        }
    };
}
