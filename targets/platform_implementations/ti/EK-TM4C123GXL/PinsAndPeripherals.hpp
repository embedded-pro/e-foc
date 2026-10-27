#pragma once

#include "hal/interfaces/DutyCycle.hpp"
#include "hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal_tiva/tiva/Adc.hpp"
#include "hal_tiva/tiva/ClockTm4c123.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "hal_tiva/tiva/PinoutTableDefaultTm4c123.hpp"
#include "hal_tiva/tiva/Pwm.hpp"
#include "targets/platform_implementations/ti/implementation/SharedGpioPin.hpp"

namespace application
{
    namespace Pins
    {
        extern hal::tiva::GpioPin currentPhaseA;
        extern hal::tiva::GpioPin currentPhaseB;
        extern hal::tiva::GpioPin currentPhaseC;
        extern hal::tiva::GpioPin powerSupplyVoltage;
        extern SharedGpioPin currentTotal;
        extern SharedGpioPin powerSupplyVoltageForUndervoltage;

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

        extern hal::tiva::GpioPin uartTx;
        extern hal::tiva::GpioPin uartRx;

        extern hal::tiva::GpioPin canRx;
        extern hal::tiva::GpioPin canTx;

        extern hal::tiva::GpioPin performance;
    }

    namespace Peripheral
    {
        using hal_pwm = hal::tiva::SynchronousPwm;

        constexpr static uint8_t QeiIndex = 0;
        constexpr static bool encoderInvertPhaseA{ false };
        constexpr static uint8_t AdcIndex = 0;
        constexpr static uint8_t AdcSequencerIndex = 0;
        constexpr static uint8_t UartIndex = 0;
        constexpr static uint8_t PwmIndex = 0;
        constexpr static uint8_t CanIndex = 0;

        constexpr static bool hasFaultComparators{ false };
        constexpr static bool hasBoardIdPins{ false };
        constexpr static bool hasPowerStatusPin{ false };
        constexpr static uint8_t OvercurrentComparatorIndex{ 0 };
        constexpr static uint8_t OvervoltageComparatorIndex{ 1 };
        constexpr static uint8_t UndervoltageComparatorIndex{ 2 };
        constexpr static float adcReferenceVoltage{ 3.3f };
        constexpr static float adcResolution{ 4096.0f };
        constexpr static uint16_t overvoltageThresholdCounts{ 0 };
        constexpr static uint16_t overcurrentThresholdCounts{ 0 };
        constexpr static uint16_t undervoltageThresholdCounts{ 0 };

        constexpr static hal::tiva::Adc::Trigger adcTrigger = hal::tiva::Adc::Trigger::pwmGenerator0;
        constexpr static hal::DutyCycle maxReadableDuty{ hal::DutyCycle::FromPercent(100) };
        constexpr static hal::DutyCycle maxDuty{ hal::DutyCycle::FromPercent(100) };

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
            hal::tiva::ConfigureClock(hal::tiva::crystalFrequency::_16_MHz, hal::tiva::oscillatorSource::main);
        }
    }
}
