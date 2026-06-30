#pragma once

#include "hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal_tiva/tiva/Adc.hpp"
#include "hal_tiva/tiva/ClockTm4c123.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "hal_tiva/tiva/PinoutTableDefaultTm4c123.hpp"
#include "hal_tiva/tiva/Pwm.hpp"

namespace application
{
    namespace Pins
    {
        static hal::tiva::GpioPin currentPhaseA{ hal::tiva::Port::E, 3 };
        static hal::tiva::GpioPin currentPhaseB{ hal::tiva::Port::E, 2 };
        static hal::tiva::GpioPin currentPhaseC{ hal::tiva::Port::E, 1 };
        static hal::tiva::GpioPin powerSupplyVoltage{ hal::tiva::Port::E, 0 };
        static hal::tiva::GpioPin currentTotal{ hal::tiva::Port::E, 0 }; // alias — no separate current-total pin on this board

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

        static hal::tiva::GpioPin operationalLed{ hal::tiva::Port::F, 1 };
        static hal::tiva::GpioPin warningLed{ hal::tiva::Port::F, 2 };
        static hal::tiva::GpioPin failureLed{ hal::tiva::Port::F, 2 }; // PF3 = canTx; alias to blue LED (PF2)

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

        // Fault comparator support is not available on EK-TM4C123GXL.
        constexpr static bool hasFaultComparators{ false };
        // boardId pins (PK0-2) and powerStatus pin (PC6) are not available on EK-TM4C123GXL.
        constexpr static bool hasBoardIdPins{ false };
        constexpr static bool hasPowerStatusPin{ false };
        constexpr static uint8_t OvercurrentComparatorIndex{ 0 };
        constexpr static uint8_t OvervoltageComparatorIndex{ 1 };
        constexpr static float adcReferenceVoltage{ 3.3f };
        constexpr static float adcResolution{ 4096.0f };
        // Hardware comparators absent — trip counts not used; set to 0 explicitly.
        constexpr static uint16_t overvoltageThresholdCounts{ 0 };
        constexpr static uint16_t overcurrentThresholdCounts{ 0 };

        static hal::tiva::Adc::Trigger adcTrigger = hal::tiva::Adc::Trigger::pwmGenerator0;

        static hal_pwm::PinChannel syncPwmPhase1{ hal_pwm::GeneratorIndex::generator0, Pins::pwmPhase1a, Pins::pwmPhase1b, true, true, std::make_optional(hal::tiva::SynchronousPwm::PinChannel::Trigger::countLoad) };
        static hal_pwm::PinChannel syncPwmPhase2{ hal_pwm::GeneratorIndex::generator1, Pins::pwmPhase2a, Pins::pwmPhase2b, true, true, std::nullopt };
        static hal_pwm::PinChannel syncPwmPhase3{ hal_pwm::GeneratorIndex::generator2, Pins::pwmPhase3a, Pins::pwmPhase3b, true, true, std::nullopt };

        static std::array<hal_pwm::PinChannel, 3> syncPwmPhases{ { syncPwmPhase1, syncPwmPhase2, syncPwmPhase3 } };

        // Async PWM stubs — not used on this board; required for compilation only.
        static hal::tiva::Pwm::PinChannel asyncPwmPhase1{ hal::tiva::Pwm::GeneratorIndex::generator0, Pins::pwmPhase1a, Pins::pwmPhase1b, true, true, std::nullopt };
        static hal::tiva::Pwm::PinChannel asyncPwmPhase2{ hal::tiva::Pwm::GeneratorIndex::generator1, Pins::pwmPhase2a, Pins::pwmPhase2b, true, true, std::nullopt };
        static hal::tiva::Pwm::PinChannel asyncPwmPhase3{ hal::tiva::Pwm::GeneratorIndex::generator2, Pins::pwmPhase3a, Pins::pwmPhase3b, true, true, std::nullopt };

        static std::array<hal::tiva::Pwm::PinChannel, 3> asyncPwmPhases{ { asyncPwmPhase1, asyncPwmPhase2, asyncPwmPhase3 } };
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
