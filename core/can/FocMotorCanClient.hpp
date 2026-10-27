#pragma once

#include "can-lite/client/CanProtocolClient.hpp"
#include "core/can/CanFrameOrigin.hpp"
#include "core/can/FocMotorCategoryClient.hpp"
#include "hal/interfaces/Can.hpp"

namespace can
{
    class FocMotorCanClient
    {
    public:
        explicit FocMotorCanClient(hal::Can& can);

        services::CanProtocolClient& ProtocolClient();
        FocMotorCategoryClient& CategoryClient();
        const FocMotorCategoryClient& CategoryClient() const;

    private:
        CanFrameOriginTracker originTracker;
        services::CanProtocolClient protocolClient;
        FocMotorCategoryClient categoryClient;
    };
}
