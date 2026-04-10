#pragma once

#include "core/foc/implementations/FocPositionImpl.hpp"
#include "core/services/cli/TerminalPosition.hpp"
#include "targets/sync_foc_sensored/instantiations/LogicWithOuterLoop.hpp"

namespace application
{
    class Logic
        : public LogicWithOuterLoop<foc::FocPositionImpl, services::TerminalFocPositionInteractor>
    {
    public:
        explicit Logic(application::PlatformFactory& hardware)
            : LogicWithOuterLoop(hardware, "sync_foc_sensored:position")
        {}
    };
}
