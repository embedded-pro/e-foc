#pragma once

#include "hal/ti/hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal/ti/hal_tiva/tiva/Gpio.hpp"
#include "hal/ti/hal_tiva/tiva/PinoutTableDefaultTm4c123.hpp"
#include "hal_tiva/tiva/Adc.hpp"
#include "hal_tiva/tiva/ClockTm4c123.hpp"

namespace application
{
    namespace Pins
    {
        static hal::tiva::GpioPin currentPhaseA{ hal::tiva::Port::E, 3 };
        static hal::tiva::GpioPin currentPhaseB{ hal::tiva::Port::E, 2 };
        static hal::tiva::GpioPin currentPhaseC{ hal::tiva::Port::E, 1 };
        static hal::tiva::GpioPin powerSupplyVoltage{ hal::tiva::Port::E, 0 };

        static hal::tiva::GpioPin hallSensorA{ hal::tiva::Port::A, 4 };
        static hal::tiva::GpioPin hallSensorB{ hal::tiva::Port::A, 5 };
        static hal::tiva::GpioPin hallSensorC{ hal::tiva::Port::A, 6 };

        static hal::tiva::GpioPin encoderA{ hal::tiva::Port::D, 6 };
        static hal::tiva::GpioPin encoderB{ hal::tiva::Port::D, 7 };
        static hal::tiva::GpioPin encoderZ{ hal::tiva::Port::D, 3 };

        static hal::tiva::GpioPin pwmPhase1a{ hal::tiva::Port::B, 6 };
        static hal::tiva::GpioPin pwmPhase1b{ hal::tiva::Port::B, 7 };
        static hal::tiva::GpioPin pwmPhase2a{ hal::tiva::Port::B, 4 };
        static hal::tiva::GpioPin pwmPhase2b{ hal::tiva::Port::B, 5 };
        static hal::tiva::GpioPin pwmPhase3a{ hal::tiva::Port::E, 4 };
        static hal::tiva::GpioPin pwmPhase3b{ hal::tiva::Port::E, 5 };

        static hal::tiva::GpioPin led1{ hal::tiva::Port::F, 1 };

        static hal::tiva::GpioPin uartTx{ hal::tiva::Port::A, 0 };
        static hal::tiva::GpioPin uartRx{ hal::tiva::Port::A, 1 };

        static hal::tiva::GpioPin canRx{ hal::tiva::Port::F, 0 };
        static hal::tiva::GpioPin canTx{ hal::tiva::Port::F, 3 };

        static hal::tiva::GpioPin performance{ hal::tiva::Port::A, 2 };
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

        static hal::tiva::Adc::Trigger adcTrigger = hal::tiva::Adc::Trigger::pwmGenerator0;

        static hal_pwm::PinChannel pwmPhase1{ hal_pwm::GeneratorIndex::generator0, Pins::pwmPhase1a, Pins::pwmPhase1b, true, true, std::make_optional(hal::tiva::SynchronousPwm::PinChannel::Trigger::countLoad) };
        static hal_pwm::PinChannel pwmPhase2{ hal_pwm::GeneratorIndex::generator1, Pins::pwmPhase2a, Pins::pwmPhase2b, true, true, std::nullopt };
        static hal_pwm::PinChannel pwmPhase3{ hal_pwm::GeneratorIndex::generator2, Pins::pwmPhase3a, Pins::pwmPhase3b, true, true, std::nullopt };

        static std::array<hal_pwm::PinChannel, 3> pwmPhases{ { pwmPhase1, pwmPhase2, pwmPhase3 } };
    }

    namespace Clocks
    {
        inline void Initialize()
        {
            hal::tiva::systemClockDivider systemClockDivisor{ 2, 5 };
            bool usesPll = true;
            hal::tiva::ConfigureClock(hal::tiva::crystalFrequency::_16_MHz, hal::tiva::oscillatorSource::main);
        }
    }
}
