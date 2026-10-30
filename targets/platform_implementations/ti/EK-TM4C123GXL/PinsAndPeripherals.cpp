#include "targets/platform_implementations/ti/EK-TM4C123GXL/PinsAndPeripherals.hpp"

namespace application
{
    namespace Pins
    {
        hal::tiva::GpioPin currentPhaseA{ hal::tiva::Port::E, 3 };
        hal::tiva::GpioPin currentPhaseB{ hal::tiva::Port::E, 2 };
        hal::tiva::GpioPin currentPhaseC{ hal::tiva::Port::E, 1 };
        hal::tiva::GpioPin powerSupplyVoltage{ hal::tiva::Port::E, 0 };
        SharedGpioPin currentTotal{ hal::tiva::Port::E, 0 }; // alias — no separate current-total pin on this board
        SharedGpioPin powerSupplyVoltageForUndervoltage{ hal::tiva::Port::E, 0 };

        hal::tiva::GpioPin hallSensorA{ hal::tiva::Port::A, 4 };
        hal::tiva::GpioPin hallSensorB{ hal::tiva::Port::A, 5 };
        hal::tiva::GpioPin hallSensorC{ hal::tiva::Port::A, 6 };

        hal::tiva::GpioPin encoderA{ hal::tiva::Port::D, 6 };
        hal::tiva::GpioPin encoderB{ hal::tiva::Port::D, 7 };
        hal::tiva::GpioPin encoderZ{ hal::tiva::Port::D, 3 };

        hal::tiva::GpioPin pwmPhase1a{ hal::tiva::Port::B, 6 };
        hal::tiva::GpioPin pwmPhase1b{ hal::tiva::Port::B, 7 };
        hal::tiva::GpioPin pwmPhase2a{ hal::tiva::Port::B, 4 };
        hal::tiva::GpioPin pwmPhase2b{ hal::tiva::Port::B, 5 };
        hal::tiva::GpioPin pwmPhase3a{ hal::tiva::Port::E, 4 };
        hal::tiva::GpioPin pwmPhase3b{ hal::tiva::Port::E, 5 };

        hal::tiva::GpioPin operationalLed{ hal::tiva::Port::F, 1 };
        hal::tiva::GpioPin warningLed{ hal::tiva::Port::F, 1 };
        hal::tiva::GpioPin failureLed{ hal::tiva::Port::F, 1 };

        hal::tiva::GpioPin boardId0{ hal::tiva::Port::D, 0 };
        hal::tiva::GpioPin boardId1{ hal::tiva::Port::D, 1 };
        hal::tiva::GpioPin boardId2{ hal::tiva::Port::D, 2 };

        hal::tiva::GpioPin uartTx{ hal::tiva::Port::A, 0 };
        hal::tiva::GpioPin uartRx{ hal::tiva::Port::A, 1 };

        hal::tiva::GpioPin canRx{ hal::tiva::Port::F, 0 };
        hal::tiva::GpioPin canTx{ hal::tiva::Port::F, 3 };

    }

    namespace Peripheral
    {
        std::array<hal_pwm::PinChannel, 3> pwmPhases{ {
            { hal_pwm::GeneratorIndex::generator0, Pins::pwmPhase1a, Pins::pwmPhase1b, true, true, std::make_optional(hal_pwm::PinChannel::Trigger::countZero) },
            { hal_pwm::GeneratorIndex::generator1, Pins::pwmPhase2a, Pins::pwmPhase2b, true, true, std::nullopt },
            { hal_pwm::GeneratorIndex::generator2, Pins::pwmPhase3a, Pins::pwmPhase3b, true, true, std::nullopt },
        } };
    }
}
