#pragma once

#include "source/application/sync_foc_sensored/instantiations/LogicWithOuterLoop.hpp"
#include "source/foc/implementations/FocSpeedImpl.hpp"
#include "source/services/cli/TerminalSpeed.hpp"

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
