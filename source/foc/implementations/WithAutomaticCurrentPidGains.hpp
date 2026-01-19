#pragma once

#include "foc/interfaces/Units.hpp"
#include "source/foc/implementations/SpeedControllerImpl.hpp"
#include "source/foc/implementations/TorqueControllerImpl.hpp"
#include "source/foc/interfaces/FieldOrientedController.hpp"
#include <numbers>

namespace foc
{
    template<typename Controller>
    class WithAutomaticCurrentPidGains
        : public Controller
    {
    public:
        using Controller::Controller;

        void SetPidBasedOnResistanceAndInductance(Volts Vdc, Ohm resistance, MilliHenry inductance, float nyquistFactor)
        {
            auto wc = CurrentLoopBandwidth(this->BaseFrequency(), nyquistFactor);
            auto kp = inductance.Value() * 0.001f * wc;
            auto ki = resistance.Value() * wc;

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

    extern template class WithAutomaticCurrentPidGains<TorqueControllerImpl>;
    extern template class WithAutomaticCurrentPidGains<SpeedControllerImpl>;

    template<typename T>
    struct IsWithAutomaticCurrentPidGains
        : std::false_type
    {
    };

    template<typename Controller>
    struct IsWithAutomaticCurrentPidGains<WithAutomaticCurrentPidGains<Controller>>
        : std::true_type
    {
    };

    template<typename T>
    constexpr bool IsWithAutomaticCurrentPidGains_v = IsWithAutomaticCurrentPidGains<T>::value;
}
