#include "targets/hardware_test/instantiations/Logic.hpp"
#include "generated/Version.h"

namespace application
{
    Logic::Logic(application::PlatformFactory& hardware)
        : terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ "hardware_test", hardware.PowerSupplyVoltage(), hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus(), e_foc::generated::VERSION, e_foc::generated::VERSION_GIT_SHA } }
        , terminal{ terminalWithStorage, hardware }
        , debugLed{ hardware.OperationalLed(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
    {}
}
