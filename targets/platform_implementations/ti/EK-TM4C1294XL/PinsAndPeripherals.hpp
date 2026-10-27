#pragma once

#include "hal/interfaces/DutyCycle.hpp"
#include "hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal_tiva/tiva/Adc.hpp"
#include "hal_tiva/tiva/ClockTm4c129.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "hal_tiva/tiva/Pwm.hpp"
#include "targets/platform_implementations/ti/implementation/SharedGpioPin.hpp"
#include MOTOR_BOARD_CHARACTERISTICS_HEADER

namespace application
{
    namespace Pins
    {
        extern hal::tiva::GpioPin currentPhaseA;
        extern hal::tiva::GpioPin currentPhaseB;
        extern hal::tiva::GpioPin currentPhaseC;
        extern hal::tiva::GpioPin powerSupplyVoltage;
        extern SharedGpioPin powerSupplyVoltageForUndervoltage;
        extern hal::tiva::GpioPin currentTotal;

        extern hal::tiva::GpioPin hallSensorA;
        extern hal::tiva::GpioPin hallSensorB;
        extern hal::tiva::GpioPin hallSensorC;

        extern hal::tiva::GpioPin encoderA;
        extern hal::tiva::GpioPin encoderB;
        extern hal::tiva::GpioPin encoderZ;

        extern hal::tiva::GpioPin pwmPhase1a;
        extern hal::tiva::GpioPin pwmPhase1b;
        extern hal::tiva::GpioPin pwmPhase2a;
        extern hal::tiva::GpioPin pwmPhase2b;
        extern hal::tiva::GpioPin pwmPhase3a;
        extern hal::tiva::GpioPin pwmPhase3b;

        extern hal::tiva::GpioPin operationalLed;
        extern hal::tiva::GpioPin warningLed;
        extern hal::tiva::GpioPin failureLed;

        extern hal::tiva::GpioPin boardId0;
        extern hal::tiva::GpioPin boardId1;
        extern hal::tiva::GpioPin boardId2;

        extern hal::tiva::GpioPin powerStatus;

        extern hal::tiva::GpioPin uartRx;
        extern hal::tiva::GpioPin uartTx;

        extern hal::tiva::GpioPin canRx;
        extern hal::tiva::GpioPin canTx;

        extern hal::tiva::GpioPin performance;
    }

    namespace Peripheral
    {
        using hal_pwm = hal::tiva::Pwm;

        constexpr static uint8_t QeiIndex = 0;
        constexpr static bool encoderInvertPhaseA{ true };
        constexpr static uint8_t AdcIndex = 0;
        constexpr static uint8_t AdcSequencerIndex = 0;
        constexpr static uint8_t UartIndex = 2;
        constexpr static uint8_t PwmIndex = 0;
        constexpr static uint8_t CanIndex = 0;

        constexpr static bool hasFaultComparators = true;
        constexpr static bool hasBoardIdPins{ true };
        constexpr static bool hasPowerStatusPin{ true };
        constexpr static uint8_t OvercurrentComparatorIndex = 0;
        constexpr static uint8_t OvervoltageComparatorIndex = 1;
        constexpr static uint8_t UndervoltageComparatorIndex = 2;

        constexpr static float adcReferenceVoltage{ 3.3f };
        constexpr static float adcResolution{ 4096.0f };

        constexpr static uint16_t overvoltageThresholdCounts =
            application::BoardCharacteristics::OvervoltageThresholdCounts(adcReferenceVoltage, adcResolution);
        constexpr static uint16_t overcurrentThresholdCounts =
            application::BoardCharacteristics::OvercurrentThresholdCounts(adcReferenceVoltage, adcResolution);
        constexpr static uint16_t undervoltageThresholdCounts =
            application::BoardCharacteristics::UndervoltageThresholdCounts(adcReferenceVoltage, adcResolution);

        constexpr static hal::tiva::Adc::Trigger adcTrigger = hal::tiva::Adc::Trigger::pwmGenerator1;
        constexpr static hal::DutyCycle maxReadableDuty{ hal::DutyCycle::FromPercent(application::BoardCharacteristics::maxReadableDutyPercent) };
        constexpr static hal::DutyCycle maxDuty{ hal::DutyCycle::FromPercent(application::BoardCharacteristics::maxDutyPercent) };

        extern std::array<hal_pwm::PinChannel, 3> pwmPhases;
    }

    namespace Resources
    {
        constexpr std::size_t tracerBufferSize{ 2048 };
        constexpr std::size_t terminalQueueSize{ 128 };
        constexpr std::size_t terminalHistorySize{ 1 };
        constexpr std::size_t terminalCommandLength{ 64 };
        constexpr std::size_t uartReceiveBufferSize{ 64 };
        constexpr std::size_t eventDispatcherSize{ 24 };
        constexpr std::size_t canReceiveBufferSize{ 16 };
        constexpr std::size_t faultStatusSize{ 448 };
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
