#pragma once

#include "hal/interfaces/DutyCycle.hpp"
#include "hal_st/stm32fxxx/DefaultClockNucleoG474xxx.hpp"
#include "hal_st/stm32fxxx/GpioStm.hpp"
#include "hal_st/stm32fxxx/PwmStm.hpp"
#include MOTOR_BOARD_CHARACTERISTICS_HEADER
#include DEVICE_HEADER
#include <array>
#include <cstddef>
#include <cstdint>

namespace application
{
    namespace Pins
    {
        extern hal::GpioPinStm currentPhaseA;
        extern hal::GpioPinStm currentPhaseB;
        extern hal::GpioPinStm currentPhaseC;
        extern hal::GpioPinStm currentTotal;
        extern hal::GpioPinStm powerSupplyVoltage;

        extern hal::GpioPinStm encoderA;
        extern hal::GpioPinStm encoderB;
        extern hal::GpioPinStm encoderZ;

        extern hal::GpioPinStm pwmPhase1a;
        extern hal::GpioPinStm pwmPhase1b;
        extern hal::GpioPinStm pwmPhase2a;
        extern hal::GpioPinStm pwmPhase2b;
        extern hal::GpioPinStm pwmPhase3a;
        extern hal::GpioPinStm pwmPhase3b;

        extern hal::GpioPinStm operationalLed;
        extern hal::GpioPinStm warningLed;
        extern hal::GpioPinStm failureLed;

        extern hal::GpioPinStm boardId0;
        extern hal::GpioPinStm boardId1;
        extern hal::GpioPinStm boardId2;

        extern hal::GpioPinStm powerStatus;

        extern hal::GpioPinStm uartRx;
        extern hal::GpioPinStm uartTx;

        extern hal::GpioPinStm canRx;
        extern hal::GpioPinStm canTx;

        extern hal::GpioPinStm eepromScl;
        extern hal::GpioPinStm eepromSda;

        extern hal::GpioPinStm performance;
    }

    namespace Peripheral
    {
        constexpr static uint8_t PwmTimerIndex{ 1 };
        constexpr static uint8_t EncoderTimerIndex{ 4 };
        constexpr static bool encoderInvertPhaseA{ true };
        constexpr static uint8_t PhaseCurrentAdcIndex{ 1 };
        constexpr static uint32_t phaseCurrentAdcTrigger{ ADC_EXTERNALTRIG_T1_TRGO };
        constexpr static uint8_t PowerSupplyAdcIndex{ 3 };
        constexpr static uint8_t DmaIndex{ 1 };
        constexpr static uint8_t PhaseCurrentDmaChannel{ 1 };
        constexpr static uint8_t phaseCurrentDmaRequest{ DMA_REQUEST_ADC1 };
        constexpr static IRQn_Type phaseCurrentDmaIrq{ DMA1_Channel1_IRQn };
        constexpr static uint8_t UartTxDmaChannel{ 2 };
        constexpr static uint8_t uartTxDmaRequest{ DMA_REQUEST_LPUART1_TX };
        constexpr static uint8_t LpUartIndex{ 1 };
        constexpr static uint8_t CanIndex{ 1 };
        constexpr static uint8_t EepromI2cIndex{ 3 };

        constexpr static bool hasBoardIdPins{ true };
        constexpr static bool hasPowerStatusPin{ true };

        constexpr static float adcReferenceVoltage{ 3.3f };
        constexpr static float adcResolution{ 4096.0f };

        constexpr static uint16_t overvoltageThresholdCounts =
            application::BoardCharacteristics::OvervoltageThresholdCounts(adcReferenceVoltage, adcResolution);
        constexpr static uint16_t overcurrentThresholdCounts =
            application::BoardCharacteristics::OvercurrentThresholdCounts(adcReferenceVoltage, adcResolution);
        constexpr static uint16_t undervoltageThresholdCounts =
            application::BoardCharacteristics::UndervoltageThresholdCounts(adcReferenceVoltage, adcResolution);

        constexpr static hal::DutyCycle maxReadableDuty{ hal::DutyCycle::FromPercent(application::BoardCharacteristics::maxReadableDutyPercent) };
        constexpr static hal::DutyCycle maxDuty{ hal::DutyCycle::FromPercent(application::BoardCharacteristics::maxDutyPercent) };

        extern const std::array<hal::PwmStmBase::ChannelConfig, 3> pwmPhases;
    }

    namespace Resources
    {
        constexpr std::size_t tracerBufferSize{ 2048 };
        constexpr std::size_t terminalQueueSize{ 128 };
        constexpr std::size_t terminalHistorySize{ 1 };
        constexpr std::size_t terminalCommandLength{ 64 };
        constexpr std::size_t eventDispatcherSize{ 24 };
        constexpr std::size_t canReceiveBufferSize{ 16 };
        constexpr std::size_t faultStatusSize{ 448 };
        constexpr std::size_t interruptTableSize{ 128 };
    }

    namespace Clocks
    {
        inline void Initialize()
        {
            ConfigureDefaultClockNucleoG474xxx();
        }
    }
}
