#pragma once

#include "hal/ti/hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal/ti/hal_tiva/tiva/Gpio.hpp"
#include "hal/ti/hal_tiva/tiva/PinoutTableDefaultTm4c129.hpp"
#include "hal_tiva/tiva/Adc.hpp"
#include "hal_tiva/tiva/ClockTm4c129.hpp"

namespace application
{
    namespace Pins
    {
        static hal::tiva::GpioPin currentPhaseA{ hal::tiva::Port::E, 3 };
        static hal::tiva::GpioPin currentPhaseB{ hal::tiva::Port::E, 2 };
        static hal::tiva::GpioPin currentPhaseC{ hal::tiva::Port::E, 1 };
        static hal::tiva::GpioPin powerSupplyVoltage{ hal::tiva::Port::B, 5 };
        static hal::tiva::GpioPin currentTotal{ hal::tiva::Port::B, 4 };

        static hal::tiva::GpioPin hallSensorA{ hal::tiva::Port::E, 4 };
        static hal::tiva::GpioPin hallSensorB{ hal::tiva::Port::E, 5 };
        static hal::tiva::GpioPin hallSensorC{ hal::tiva::Port::E, 6 };

        static hal::tiva::GpioPin encoderA{ hal::tiva::Port::L, 1 };
        static hal::tiva::GpioPin encoderB{ hal::tiva::Port::L, 2 };
        static hal::tiva::GpioPin encoderZ{ hal::tiva::Port::L, 3 };

        static hal::tiva::GpioPin pwmPhase1a{ hal::tiva::Port::F, 2 };
        static hal::tiva::GpioPin pwmPhase1b{ hal::tiva::Port::F, 3 };
        static hal::tiva::GpioPin pwmPhase2a{ hal::tiva::Port::G, 0 };
        static hal::tiva::GpioPin pwmPhase2b{ hal::tiva::Port::G, 1 };
        static hal::tiva::GpioPin pwmPhase3a{ hal::tiva::Port::K, 4 };
        static hal::tiva::GpioPin pwmPhase3b{ hal::tiva::Port::K, 5 };

        static hal::tiva::GpioPin led1{ hal::tiva::Port::N, 0 };

        static hal::tiva::GpioPin uartRx{ hal::tiva::Port::A, 0 };
        static hal::tiva::GpioPin uartTx{ hal::tiva::Port::A, 1 };

        static hal::tiva::GpioPin canRx{ hal::tiva::Port::B, 0 };
        static hal::tiva::GpioPin canTx{ hal::tiva::Port::B, 1 };

        static hal::tiva::GpioPin performance{ hal::tiva::Port::N, 4 };
    }

    namespace Peripheral
    {
        using hal_pwm = hal::tiva::SynchronousPwm;

        constexpr static uint8_t QeiIndex = 0;
        constexpr static uint8_t AdcIndex = 0;
        constexpr static uint8_t AdcSequencerIndex = 0;
        constexpr static uint8_t UartIndex = 0;
        constexpr static uint8_t PwmIndex = 0;
        constexpr static uint8_t CanIndex = 0;

        static hal::tiva::Adc::Trigger adcTrigger = hal::tiva::Adc::Trigger::pwmGenerator1;

        static hal_pwm::PinChannel pwmPhase1{ hal_pwm::GeneratorIndex::generator1, Pins::pwmPhase1a, Pins::pwmPhase1b, true, true, std::make_optional(hal::tiva::SynchronousPwm::PinChannel::Trigger::countZero) };
        static hal_pwm::PinChannel pwmPhase2{ hal_pwm::GeneratorIndex::generator2, Pins::pwmPhase2a, Pins::pwmPhase2b, true, true, std::nullopt };
        static hal_pwm::PinChannel pwmPhase3{ hal_pwm::GeneratorIndex::generator3, Pins::pwmPhase3a, Pins::pwmPhase3b, true, true, std::nullopt };

        static std::array<hal_pwm::PinChannel, 3> pwmPhases{ { pwmPhase1, pwmPhase2, pwmPhase3 } };
    }

    namespace Clocks
    {
        inline void Initialize()
        {
            uint32_t frequency = 120000000;
            hal::tiva::crystalFrequency hseValue = hal::tiva::crystalFrequency::_25_MHz;
            hal::tiva::oscillatorSource oscSource = hal::tiva::oscillatorSource::main;
            hal::tiva::systemClockVco systemClockVco = hal::tiva::systemClockVco::_240_MHz;
            bool usesPll = true;
            hal::tiva::ConfigureClock(frequency, hseValue, oscSource, systemClockVco, usesPll);
        }
    }
}
