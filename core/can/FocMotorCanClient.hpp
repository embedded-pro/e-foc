#pragma once

#include "can-lite/client/CanProtocolClient.hpp"
#include "core/can/FocMotorCategoryClient.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "hal/interfaces/Can.hpp"
#include <cstdint>

namespace can
{
    class FocMotorCanClient
    {
    public:
        FocMotorCanClient(hal::Can& can, uint16_t nodeId);

        bool Start();
        bool Stop();
        bool ClearFault();
        bool EmergencyStop();
        bool SelectControlMode(FocMotorMode mode);
        bool SetTorque(foc::Ampere value);
        bool SetSpeed(foc::RadiansPerSecond value);
        bool SetPosition(foc::Radians value);

        bool SetCurrentBandwidth(float bandwidth);
        bool SetSpeedBandwidth(float bandwidth);
        bool SetPositionBandwidth(float bandwidth);

        services::CanProtocolClient& ProtocolClient();
        FocMotorCategoryClient& CategoryClient();

    private:
        uint16_t nodeId;
        services::CanProtocolClient protocolClient;
        FocMotorCategoryClient categoryClient;
    };
}
