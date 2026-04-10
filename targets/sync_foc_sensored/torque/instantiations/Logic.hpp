#pragma once

#include "core/foc/implementations/FocTorqueImpl.hpp"
#include "core/platform_abstraction/MotorFieldOrientedControllerAdapter.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/cli/TerminalTorque.hpp"
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

    class Logic
    {
    public:
        explicit Logic(application::PlatformFactory& hardware);

    private:
        PlatformAdapter platformAdapter;
        services::DebugLed debugLed;
        foc::Volts vdc;
        services::TerminalWithBanner::WithMaxSize<20> terminalWithStorage;
        services::NvmEepromRegion calibrationRegion;
        services::NvmEepromRegion configRegion;
        services::NonVolatileMemoryImpl nvm;
        services::ElectricalParametersIdentificationImpl electricalIdent;
        services::MotorAlignmentImpl motorAlignment;
        state_machine::NoOpFaultNotifier noOpFaultNotifier;
        FocStateMachineImpl<foc::FocTorqueImpl, services::TerminalFocTorqueInteractor, SelectedTransitionPolicy> motorStateMachine;
    };
}
