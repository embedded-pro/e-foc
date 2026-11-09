#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "can-lite/tracing/TracingBypassCan.hpp"
#include "can-lite/tracing/TracingCan.hpp"
#include "can-lite/tracing/TracingCanProtocolServerObserver.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include <cstdint>

namespace application
{
    struct CanStack
    {
        CanStack(PlatformFactory& hardware, uint16_t nodeId);

        services::TracingCan tracingCan;
        services::TracingBypassCan canBus;
        services::CanProtocolServer canServer;
        services::TracingCanProtocolServerObserver tracingServerObserver;
        can::FocMotorCategoryServer motorCanServer;
        services::FirmwareUpgradeCategoryServer firmwareUpgradeServer;
    };
}
