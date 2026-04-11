#include "core/state_machine/FocStateMachineImpl.hpp"
#include "core/foc/implementations/FocPositionImpl.hpp"
#include "core/foc/implementations/FocSpeedImpl.hpp"
#include "core/foc/implementations/FocTorqueImpl.hpp"
#include "core/services/cli/TerminalPosition.hpp"
#include "core/services/cli/TerminalSpeed.hpp"
#include "core/services/cli/TerminalTorque.hpp"

namespace application
{
    template class FocStateMachineImpl<foc::FocTorqueImpl, services::TerminalFocTorqueInteractor, state_machine::CliTransitionPolicy>;
    template class FocStateMachineImpl<foc::FocTorqueImpl, services::TerminalFocTorqueInteractor, state_machine::AutoTransitionPolicy>;
    template class FocStateMachineImpl<foc::FocSpeedImpl, services::TerminalFocSpeedInteractor, state_machine::CliTransitionPolicy>;
    template class FocStateMachineImpl<foc::FocSpeedImpl, services::TerminalFocSpeedInteractor, state_machine::AutoTransitionPolicy>;
    template class FocStateMachineImpl<foc::FocPositionImpl, services::TerminalFocPositionInteractor, state_machine::CliTransitionPolicy>;
    template class FocStateMachineImpl<foc::FocPositionImpl, services::TerminalFocPositionInteractor, state_machine::AutoTransitionPolicy>;
}
