#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeDefinitions.hpp"
#include <cstdint>
#include <string>

namespace tool
{
    enum class FirmwareStep : uint8_t
    {
        download,
        apply
    };

    std::string FirmwareRefusalText(services::FwuError error, FirmwareStep step);
}
