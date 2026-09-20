#pragma once

#include "hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal_tiva/tiva/Adc.hpp"
#include "hal_tiva/tiva/ClockTm4c129.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "hal_tiva/tiva/Pwm.hpp"
#include MOTOR_BOARD_CHARACTERISTICS_HEADER

namespace application
{
    namespace Pins
    {
        inline hal::tiva::GpioPin currentPhaseA{ hal::tiva::Port::E, 3 };
        inline hal::tiva::GpioPin currentPhaseB{ hal::tiva::Port::E, 2 };
        inline hal::tiva::GpioPin currentPhaseC{ hal::tiva::Port::E, 1 };
        inline hal::tiva::GpioPin powerSupplyVoltage{ hal::tiva::Port::B, 5 };
        inline hal::tiva::GpioPin currentTotal{ hal::tiva::Port::B, 4 };

        inline hal::tiva::GpioPin hallSensorA{ hal::tiva::Port::E, 4 };
        inline hal::tiva::GpioPin hallSensorB{ hal::tiva::Port::E, 5 };
        inline hal::tiva::GpioPin hallSensorC{ hal::tiva::Port::E, 6 };

        inline hal::tiva::GpioPin encoderA{ hal::tiva::Port::L, 1 };
        inline hal::tiva::GpioPin encoderB{ hal::tiva::Port::L, 2 };
        inline hal::tiva::GpioPin encoderZ{ hal::tiva::Port::L, 3 };

        inline hal::tiva::GpioPin pwmPhase1a{ hal::tiva::Port::F, 2 };
        inline hal::tiva::GpioPin pwmPhase1b{ hal::tiva::Port::F, 3 };
        inline hal::tiva::GpioPin pwmPhase2a{ hal::tiva::Port::G, 0 };
        inline hal::tiva::GpioPin pwmPhase2b{ hal::tiva::Port::G, 1 };
        inline hal::tiva::GpioPin pwmPhase3a{ hal::tiva::Port::K, 4 };
        inline hal::tiva::GpioPin pwmPhase3b{ hal::tiva::Port::K, 5 };

        inline hal::tiva::GpioPin operationalLed{ hal::tiva::Port::N, 3 };
        inline hal::tiva::GpioPin warningLed{ hal::tiva::Port::N, 2 };
        inline hal::tiva::GpioPin failureLed{ hal::tiva::Port::P, 2 };

        inline hal::tiva::GpioPin boardId0{ hal::tiva::Port::K, 0, hal::tiva::Drive::Up };
        inline hal::tiva::GpioPin boardId1{ hal::tiva::Port::K, 1, hal::tiva::Drive::Up };
        inline hal::tiva::GpioPin boardId2{ hal::tiva::Port::K, 2, hal::tiva::Drive::Up };

        inline hal::tiva::GpioPin powerStatus{ hal::tiva::Port::C, 6, hal::tiva::Drive::Up };

        inline hal::tiva::GpioPin uartRx{ hal::tiva::Port::D, 4 };
        inline hal::tiva::GpioPin uartTx{ hal::tiva::Port::D, 5 };

        inline hal::tiva::GpioPin canRx{ hal::tiva::Port::A, 0 };
        inline hal::tiva::GpioPin canTx{ hal::tiva::Port::A, 1 };

        inline hal::tiva::GpioPin performance{ hal::tiva::Port::N, 4 };
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
        constexpr static bool hasBoardIdPins{ true };
        constexpr static bool hasPowerStatusPin{ true };
        constexpr static uint8_t OvercurrentComparatorIndex = 0;
        constexpr static uint8_t OvervoltageComparatorIndex = 1;

        constexpr static float adcReferenceVoltage{ 3.3f };
        constexpr static float adcResolution{ 4096.0f };

        // Fault trip thresholds derived from motor board shield characteristics.
        constexpr static uint16_t overvoltageThresholdCounts =
            application::BoardCharacteristics::OvervoltageThresholdCounts(adcReferenceVoltage, adcResolution);
        constexpr static uint16_t overcurrentThresholdCounts =
            application::BoardCharacteristics::OvercurrentThresholdCounts(adcResolution);

        constexpr static hal::tiva::Adc::Trigger adcTrigger = hal::tiva::Adc::Trigger::pwmGenerator1;

        inline std::array<hal_pwm::PinChannel, 3> pwmPhases{ {
            { hal_pwm::GeneratorIndex::generator1, Pins::pwmPhase1a, Pins::pwmPhase1b, true, true, std::make_optional(hal_pwm::PinChannel::Trigger::countZero) },
            { hal_pwm::GeneratorIndex::generator2, Pins::pwmPhase2a, Pins::pwmPhase2b, true, true, std::nullopt },
            { hal_pwm::GeneratorIndex::generator3, Pins::pwmPhase3a, Pins::pwmPhase3b, true, true, std::nullopt },
        } };
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
