#pragma once

#include "source/foc/implementations/FocSpeedImpl.hpp"
#include "source/services/cli/TerminalSpeed.hpp"
#include "targets/sync_foc_sensored/instantiations/LogicWithOuterLoop.hpp"

namespace application
{
    class Logic
        : public LogicWithOuterLoop<foc::FocSpeedImpl, services::TerminalFocSpeedInteractor>
    {
    public:
        explicit Logic(application::HardwareFactory& hardware)
            : LogicWithOuterLoop(hardware, "sync_foc_sensored:speed")
        {}
    };
}
