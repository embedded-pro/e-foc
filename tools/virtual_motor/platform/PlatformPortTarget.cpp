#include "tools/virtual_motor/platform/PlatformPortTarget.hpp"

namespace virtual_motor
{
    PlatformPortTarget::PlatformPortTarget(VirtualPlatform& platform)
        : platform(platform)
    {}

    port::Hello PlatformPortTarget::Identity() const
    {
        const auto& options = platform.Options();
        return { port::protocolVersion, options.node, options.bus, options.plantName, platform.BaseFrequency().Value() };
    }

    port::Status PlatformPortTarget::CurrentStatus() const
    {
        const auto& runner = platform.Runner();
        return {
            platform.ControlPeriods(),
            runner.RealTimeFactor(),
            runner.Overruns(),
            runner.Paused() ? port::RunState::paused : port::RunState::running,
            platform.LedStates(),
            static_cast<uint8_t>(platform.BoardProtectionStatus()),
        };
    }

    port::Changes PlatformPortTarget::ChangesInForce() const
    {
        return platform.Disturbances().InForce();
    }

    void PlatformPortTarget::Pause()
    {
        platform.Runner().Pause();
    }

    void PlatformPortTarget::Resume()
    {
        platform.Runner().Resume();
    }

    std::optional<port::RefusalReason> PlatformPortTarget::Step(port::StepUnit unit, uint16_t count)
    {
        return platform.Runner().Step(unit, count);
    }

    std::optional<port::RefusalReason> PlatformPortTarget::SetLoadTorque(float newtonMeter)
    {
        return platform.Disturbances().SetLoadTorque(newtonMeter);
    }

    std::optional<port::RefusalReason> PlatformPortTarget::SetExternalTorque(const port::ExternalTorque& torque)
    {
        return platform.Disturbances().SetExternalTorque(torque);
    }

    std::optional<port::RefusalReason> PlatformPortTarget::SetCurrentNoise(float ampere)
    {
        return platform.Disturbances().SetCurrentNoise(ampere);
    }

    std::optional<port::RefusalReason> PlatformPortTarget::SetEncoderNoise(float radians)
    {
        return platform.Disturbances().SetEncoderNoise(radians);
    }

    void PlatformPortTarget::ClearAll()
    {
        platform.Disturbances().ClearAll();
    }

    void PlatformPortTarget::Attach(InstrumentReports& reports)
    {
        platform.Instruments().Attach(reports);
    }

    void PlatformPortTarget::Detach()
    {
        platform.Instruments().Detach(platform.ControlPeriods());
    }

    void PlatformPortTarget::Stream(bool on)
    {
        platform.Instruments().Stream(on);
    }

    std::optional<port::RefusalReason> PlatformPortTarget::Capture(const port::CaptureRequest& request)
    {
        return platform.Instruments().Capture(request, platform.ControlPeriods());
    }

    void PlatformPortTarget::CancelCapture()
    {
        platform.Instruments().CancelCapture(platform.ControlPeriods());
    }

    std::optional<port::RefusalReason> PlatformPortTarget::RunProfile(const port::ProfileRequest& profile)
    {
        return platform.Instruments().RunProfile(profile);
    }

    void PlatformPortTarget::StopProfile()
    {
        platform.Instruments().StopProfile();
    }
}
