#include "targets/platform_implementations/st/NUCLEO-G474RE/PinsAndPeripherals.hpp"

unsigned int hse_value = 24'000'000;

namespace application
{
    namespace Pins
    {
        hal::GpioPinStm currentPhaseA{ hal::Port::A, 0 };
        hal::GpioPinStm currentPhaseB{ hal::Port::A, 1 };
        hal::GpioPinStm currentPhaseC{ hal::Port::C, 0 };
        hal::GpioPinStm currentTotal{ hal::Port::C, 1 };
        hal::GpioPinStm powerSupplyVoltage{ hal::Port::B, 0 };

        hal::GpioPinStm encoderA{ hal::Port::B, 6 };
        hal::GpioPinStm encoderB{ hal::Port::B, 7 };
        hal::GpioPinStm encoderZ{ hal::Port::D, 2 };

        hal::GpioPinStm pwmPhase1a{ hal::Port::A, 8 };
        hal::GpioPinStm pwmPhase1b{ hal::Port::B, 13 };
        hal::GpioPinStm pwmPhase2a{ hal::Port::A, 9 };
        hal::GpioPinStm pwmPhase2b{ hal::Port::B, 14 };
        hal::GpioPinStm pwmPhase3a{ hal::Port::A, 10 };
        hal::GpioPinStm pwmPhase3b{ hal::Port::B, 15 };

        hal::GpioPinStm operationalLed{ hal::Port::B, 5 };
        hal::GpioPinStm warningLed{ hal::Port::B, 4 };
        hal::GpioPinStm failureLed{ hal::Port::B, 10 };

        hal::GpioPinStm boardId0{ hal::Port::C, 10, hal::Drive::Default, hal::Speed::Default, hal::WeakPull::Up };
        hal::GpioPinStm boardId1{ hal::Port::C, 11, hal::Drive::Default, hal::Speed::Default, hal::WeakPull::Up };
        hal::GpioPinStm boardId2{ hal::Port::C, 12, hal::Drive::Default, hal::Speed::Default, hal::WeakPull::Up };

        hal::GpioPinStm powerStatus{ hal::Port::B, 2, hal::Drive::Default, hal::Speed::Default, hal::WeakPull::Up };

        hal::GpioPinStm uartRx{ hal::Port::A, 3 };
        hal::GpioPinStm uartTx{ hal::Port::A, 2 };

        hal::GpioPinStm canRx{ hal::Port::A, 11 };
        hal::GpioPinStm canTx{ hal::Port::A, 12 };

        hal::GpioPinStm eepromScl{ hal::Port::C, 8 };
        hal::GpioPinStm eepromSda{ hal::Port::C, 9 };

        hal::GpioPinStm performance{ hal::Port::C, 6 };
    }

    namespace Peripheral
    {
        const std::array<hal::PwmStmBase::ChannelConfig, 3> pwmPhases{ {
            { 1, Pins::pwmPhase1a, Pins::pwmPhase1b },
            { 2, Pins::pwmPhase2a, Pins::pwmPhase2b },
            { 3, Pins::pwmPhase3a, Pins::pwmPhase3b },
        } };
    }
}
