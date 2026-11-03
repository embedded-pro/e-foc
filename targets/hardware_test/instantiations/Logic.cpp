#include "targets/hardware_test/instantiations/Logic.hpp"
#include "e_foc/BuildInfo.hpp"

namespace application
{
    Logic::Logic(application::PlatformFactory& hardware)
        : terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ "hardware_test", hardware.PowerSupplyVoltage(), hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus(), build_info::version, build_info::revision } }
        , terminal{ terminalWithStorage, hardware }
        , debugLed{ hardware.OperationalLed(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
    {}
}
