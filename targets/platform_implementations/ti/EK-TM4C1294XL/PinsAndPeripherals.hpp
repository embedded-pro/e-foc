#pragma once

#include "hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal_tiva/tiva/Adc.hpp"
#include "hal_tiva/tiva/ClockTm4c129.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "hal_tiva/tiva/PinoutTableDefaultTm4c129.hpp"
#include "hal_tiva/tiva/Pwm.hpp"

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

        static hal::tiva::GpioPin uartRx{ hal::tiva::Port::D, 4 };
        static hal::tiva::GpioPin uartTx{ hal::tiva::Port::D, 5 };

        static hal::tiva::GpioPin canRx{ hal::tiva::Port::A, 0 };
        static hal::tiva::GpioPin canTx{ hal::tiva::Port::A, 1 };

        static hal::tiva::GpioPin performance{ hal::tiva::Port::N, 4 };
    }

    namespace Peripheral
    {
        using hal_pwm = hal::tiva::Pwm;

        constexpr static uint8_t QeiIndex = 0;
        constexpr static uint8_t AdcIndex = 0;
        constexpr static uint8_t AdcSequencerIndex = 0;
        constexpr static uint8_t UartIndex = 2;
        constexpr static uint8_t PwmIndex = 0;
        constexpr static uint8_t CanIndex = 0;

        // ADC digital comparator indices mapped to PWM FLTSRC1 lines.
        // DCMP0 (PB4 / ADC10) → overcurrent trip; DCMP1 (PB5 / ADC11) → overvoltage trip.
        constexpr static bool hasFaultComparators = true;
        constexpr static uint8_t OvercurrentComparatorIndex = 0;
        constexpr static uint8_t OvervoltageComparatorIndex = 1;

        // Overvoltage: bus voltage limit 58 V; divider factor = 18.433.
        // count = (58 / (3.3 * 18.433)) * 4096
        constexpr static float adcReferenceVoltage = 3.3f;
        constexpr static float adcResolution = 4096.0f;
        constexpr static float voltageToVolts = 18.433f;
        constexpr static float overvoltageThresholdVolts = 58.0f;
        constexpr static uint16_t overvoltageThresholdCounts = static_cast<uint16_t>(
            (overvoltageThresholdVolts / (adcReferenceVoltage * voltageToVolts)) * adcResolution);

        // Overcurrent on PB4: assume 0–15 A sensor → 0–3.3 V; trip at 12 A (80 %).
        constexpr static float maxCurrentAmps = 15.0f;
        constexpr static float overcurrentThresholdAmps = 12.0f;
        constexpr static uint16_t overcurrentThresholdCounts = static_cast<uint16_t>(
            (overcurrentThresholdAmps / maxCurrentAmps) * (adcResolution - 1.0f));

        static hal::tiva::Adc::Trigger adcTrigger = hal::tiva::Adc::Trigger::pwmGenerator1;

        static hal_pwm::PinChannel asyncPwmPhase1{ hal_pwm::GeneratorIndex::generator1, Pins::pwmPhase1a, Pins::pwmPhase1b, true, true, std::make_optional(hal::tiva::Pwm::PinChannel::Trigger::countZero) };
        static hal_pwm::PinChannel asyncPwmPhase2{ hal_pwm::GeneratorIndex::generator2, Pins::pwmPhase2a, Pins::pwmPhase2b, true, true, std::nullopt };
        static hal_pwm::PinChannel asyncPwmPhase3{ hal_pwm::GeneratorIndex::generator3, Pins::pwmPhase3a, Pins::pwmPhase3b, true, true, std::nullopt };

        static std::array<hal_pwm::PinChannel, 3> asyncPwmPhases{ { asyncPwmPhase1, asyncPwmPhase2, asyncPwmPhase3 } };

        // Synchronous PWM stubs — not used on this board; required for compilation only.
        static hal::tiva::SynchronousPwm::PinChannel syncPwmPhase1{ hal::tiva::SynchronousPwm::GeneratorIndex::generator0, Pins::pwmPhase1a, Pins::pwmPhase1b, true, true, std::nullopt };
        static hal::tiva::SynchronousPwm::PinChannel syncPwmPhase2{ hal::tiva::SynchronousPwm::GeneratorIndex::generator1, Pins::pwmPhase2a, Pins::pwmPhase2b, true, true, std::nullopt };
        static hal::tiva::SynchronousPwm::PinChannel syncPwmPhase3{ hal::tiva::SynchronousPwm::GeneratorIndex::generator2, Pins::pwmPhase3a, Pins::pwmPhase3b, true, true, std::nullopt };

        static std::array<hal::tiva::SynchronousPwm::PinChannel, 3> syncPwmPhases{ { syncPwmPhase1, syncPwmPhase2, syncPwmPhase3 } };
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
