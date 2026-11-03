#pragma once

#include "core/services/firmware_upgrade/FirmwareUpgradeControl.hpp"
#include "gmock/gmock.h"

namespace services
{
    class FirmwareUpgradeControlMock
        : public FirmwareUpgradeControl
    {
    public:
        MOCK_METHOD(bool, IsRunning, (), (const, override));
        MOCK_METHOD(FirmwareVersion, StagedVersion, (), (const, override));
        MOCK_METHOD(void, EmergencyStop, (), (override));
    };
}
