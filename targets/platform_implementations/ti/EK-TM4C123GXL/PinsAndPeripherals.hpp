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
        inline hal::tiva::GpioPin currentPhaseA{ hal::tiva::Port::E, 3 };
        inline hal::tiva::GpioPin currentPhaseB{ hal::tiva::Port::E, 2 };
        inline hal::tiva::GpioPin currentPhaseC{ hal::tiva::Port::E, 1 };
        inline hal::tiva::GpioPin powerSupplyVoltage{ hal::tiva::Port::E, 0 };
        inline hal::tiva::GpioPin currentTotal{ hal::tiva::Port::E, 0 }; // alias — no separate current-total pin on this board

        inline hal::tiva::GpioPin hallSensorA{ hal::tiva::Port::A, 4 };
        inline hal::tiva::GpioPin hallSensorB{ hal::tiva::Port::A, 5 };
        inline hal::tiva::GpioPin hallSensorC{ hal::tiva::Port::A, 6 };

        inline hal::tiva::GpioPin encoderA{ hal::tiva::Port::D, 6 };
        inline hal::tiva::GpioPin encoderB{ hal::tiva::Port::D, 7 };
        inline hal::tiva::GpioPin encoderZ{ hal::tiva::Port::D, 3 };

        inline hal::tiva::GpioPin pwmPhase1a{ hal::tiva::Port::B, 6 };
        inline hal::tiva::GpioPin pwmPhase1b{ hal::tiva::Port::B, 7 };
        inline hal::tiva::GpioPin pwmPhase2a{ hal::tiva::Port::B, 4 };
        inline hal::tiva::GpioPin pwmPhase2b{ hal::tiva::Port::B, 5 };
        inline hal::tiva::GpioPin pwmPhase3a{ hal::tiva::Port::E, 4 };
        inline hal::tiva::GpioPin pwmPhase3b{ hal::tiva::Port::E, 5 };

        inline hal::tiva::GpioPin operationalLed{ hal::tiva::Port::F, 1 };
        inline hal::tiva::GpioPin warningLed{ hal::tiva::Port::F, 1 };
        inline hal::tiva::GpioPin failureLed{ hal::tiva::Port::F, 1 };

        inline hal::tiva::GpioPin boardId0{ hal::tiva::Port::D, 0 };
        inline hal::tiva::GpioPin boardId1{ hal::tiva::Port::D, 1 };
        inline hal::tiva::GpioPin boardId2{ hal::tiva::Port::D, 2 };
        inline hal::tiva::GpioPin powerStatus{ hal::tiva::Port::D, 4 };

        inline hal::tiva::GpioPin uartTx{ hal::tiva::Port::A, 0 };
        inline hal::tiva::GpioPin uartRx{ hal::tiva::Port::A, 1 };

        inline hal::tiva::GpioPin canRx{ hal::tiva::Port::F, 0 };
        inline hal::tiva::GpioPin canTx{ hal::tiva::Port::F, 3 };

        inline hal::tiva::GpioPin performance{ hal::tiva::Port::A, 2 };
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
        constexpr static bool hasBoardIdPins{ false };
        constexpr static bool hasPowerStatusPin{ false };
        constexpr static uint8_t OvercurrentComparatorIndex{ 0 };
        constexpr static uint8_t OvervoltageComparatorIndex{ 1 };
        constexpr static float adcReferenceVoltage{ 3.3f };
        constexpr static float adcResolution{ 4096.0f };
        // Hardware comparators absent — trip counts not used; set to 0 explicitly.
        constexpr static uint16_t overvoltageThresholdCounts{ 0 };
        constexpr static uint16_t overcurrentThresholdCounts{ 0 };

        constexpr static hal::tiva::Adc::Trigger adcTrigger = hal::tiva::Adc::Trigger::pwmGenerator0;

        inline std::array<hal_pwm::PinChannel, 3> pwmPhases{ {
            { hal_pwm::GeneratorIndex::generator0, Pins::pwmPhase1a, Pins::pwmPhase1b, true, true, std::make_optional(hal_pwm::PinChannel::Trigger::countLoad) },
            { hal_pwm::GeneratorIndex::generator1, Pins::pwmPhase2a, Pins::pwmPhase2b, true, true, std::nullopt },
            { hal_pwm::GeneratorIndex::generator2, Pins::pwmPhase3a, Pins::pwmPhase3b, true, true, std::nullopt },
        } };
    }

    // Buffer capacities for the shared platform implementation. This part has 32 KB of SRAM against
    // the TM4C1294's 256 KB, so it cannot afford that board's tracer and history depth.
    namespace Resources
    {
        constexpr std::size_t tracerBufferSize{ 2048 };
        constexpr std::size_t terminalQueueSize{ 256 };
        constexpr std::size_t terminalHistorySize{ 4 };
        constexpr std::size_t uartReceiveBufferSize{ 256 };
        constexpr std::size_t eventDispatcherSize{ 50 };
        constexpr std::size_t canReceiveBufferSize{ 32 };
        constexpr std::size_t faultStatusSize{ 1024 };
    }

    namespace Clocks
    {
        inline void Initialize()
        {
            hal::tiva::ConfigureClock(hal::tiva::crystalFrequency::_16_MHz, hal::tiva::oscillatorSource::main);
        }
    }
}
