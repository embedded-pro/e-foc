#include "core/state_machine/FocStateMachineImpl.hpp"
#include "core/foc/implementations/FocPositionImpl.hpp"
#include "core/foc/implementations/FocSpeedImpl.hpp"
#include "core/foc/implementations/FocTorqueImpl.hpp"

namespace application
{
    template class FocStateMachineImpl<foc::FocTorqueImpl>;
    template class FocStateMachineImpl<foc::FocSpeedImpl>;
    template class FocStateMachineImpl<foc::FocPositionImpl>;
}
