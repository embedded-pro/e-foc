#include "core/services/cli/StartupBanner.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"

namespace services
{
    void TraceStartupBanner(services::Tracer& tracer, const StartupBanner& banner)
    {
        tracer.Trace() << "\033[2J\033[H";
        tracer.Trace() << "================================================";
        tracer.Trace() << "e-foc:" << banner.targetName;
        tracer.Trace() << "Version: " << banner.version;
        tracer.Trace() << "Revision: " << banner.revision;
        tracer.Trace() << "Build: " << __DATE__ << " " << __TIME__;
#ifdef __VERSION__
        tracer.Trace() << "Compiler: " << __VERSION__;
#elif defined(_MSC_VER)
        tracer.Trace() << "Compiler: MSVC " << _MSC_VER;
#else
        tracer.Trace() << "Compiler: Unknown";
#endif
        tracer.Trace() << "Target: " << E_FOC_TARGET_BOARD;
        tracer.Trace() << "System Clock: " << banner.systemClock.Value() << " Hz";
        tracer.Trace() << "Power Supply Voltage: " << banner.vdc.Value() << " V";
        tracer.Trace() << "Reset Cause: " << banner.resetCause;
        tracer.Trace() << "================================================";
        if (!banner.faultStatus.empty())
        {
            tracer.Trace() << "!!! FAULT DATA FROM PREVIOUS SESSION !!!";
            tracer.Trace() << banner.faultStatus;
            tracer.Trace() << "!!! END FAULT DATA !!!";
        }
    }
}
