#pragma once

#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/cli/TerminalWithBanner.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "core/services/non_volatile_memory/NvmEepromRegion.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocStateMachineImpl.hpp"
#include "core/state_machine/TransitionPolicies.hpp"
#include "services/peripheral/DebugLed.hpp"

namespace application
{
#ifdef E_FOC_AUTO_TRANSITION_POLICY
    using SelectedTransitionPolicy = state_machine::AutoTransitionPolicy;
#else
    using SelectedTransitionPolicy = state_machine::CliTransitionPolicy;
#endif

    template<typename FocImpl, typename TerminalImpl>
    class LogicWithOuterLoop
    {
    public:
        explicit LogicWithOuterLoop(application::PlatformFactory& hardware, infra::BoundedConstString bannerName)
            : debugLed{ hardware.Leds().front(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
            , vdc{ hardware.PowerSupplyVoltage() }
            , terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ bannerName, vdc, hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus() } }
            , calibrationRegion{ hardware.Eeprom(), 0, 128 }
            , configRegion{ hardware.Eeprom(), 128, 128 }
            , nvm{ calibrationRegion, configRegion }
            , electricalIdent{ hardware, hardware, vdc }
            , motorAlignment{ hardware, hardware }
            , motorStateMachine(
                  TerminalAndTracer{ terminalWithStorage, hardware.Tracer() },
                  MotorHardware{ hardware, hardware, vdc },
                  nvm,
                  CalibrationServices{ electricalIdent, motorAlignment },
                  noOpFaultNotifier,
                  hardware.MaxCurrentSupported(), hardware.BaseFrequency(), hardware.LowPriorityInterrupt())
        {
            hardware.ConfigureAdcAndPwm(hal::Hertz{ 10000 }, std::chrono::nanoseconds{ 500 }, PlatformFactory::SampleAndHold::shorter);
            hardware.SetEncoderResolution(4000);
            hardware.ConfigureCanBus(1'000'000, false);
        }

    private:
        services::DebugLed debugLed;
        foc::Volts vdc;
        services::TerminalWithBanner::WithMaxSize<20> terminalWithStorage;
        services::NvmEepromRegion calibrationRegion;
        services::NvmEepromRegion configRegion;
        services::NonVolatileMemoryImpl nvm;
        services::ElectricalParametersIdentificationImpl electricalIdent;
        services::MotorAlignmentImpl motorAlignment;
        state_machine::NoOpFaultNotifier noOpFaultNotifier;
        FocStateMachineImpl<FocImpl, TerminalImpl, SelectedTransitionPolicy> motorStateMachine;
    };
}
