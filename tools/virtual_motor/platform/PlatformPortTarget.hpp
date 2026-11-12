#pragma once

#include "tools/virtual_motor/platform/PlantPortClient.hpp"
#include "tools/virtual_motor/platform/VirtualPlatform.hpp"

namespace virtual_motor
{
    class PlatformPortTarget
        : public PlantPortTarget
    {
    public:
        explicit PlatformPortTarget(VirtualPlatform& platform);

        port::Hello Identity() const override;
        port::Status CurrentStatus() const override;
        port::Changes ChangesInForce() const override;

        void Pause() override;
        void Resume() override;
        std::optional<port::RefusalReason> Step(port::StepUnit unit, uint16_t count) override;

        std::optional<port::RefusalReason> SetLoadTorque(float newtonMeter) override;
        std::optional<port::RefusalReason> SetExternalTorque(const port::ExternalTorque& torque) override;
        std::optional<port::RefusalReason> SetCurrentNoise(float ampere) override;
        std::optional<port::RefusalReason> SetEncoderNoise(float radians) override;
        void ClearAll() override;

        void Attach(InstrumentReports& reports) override;
        void Detach() override;
        void Stream(bool on) override;
        std::optional<port::RefusalReason> Capture(const port::CaptureRequest& request) override;
        void CancelCapture() override;
        std::optional<port::RefusalReason> RunProfile(const port::ProfileRequest& profile) override;
        void StopProfile() override;

    private:
        VirtualPlatform& platform;
    };
}
