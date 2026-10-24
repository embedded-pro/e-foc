#pragma once

#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include <cstdint>
#include <string_view>

namespace tool
{
    std::string_view CommandDisplayName(uint8_t categoryId, uint8_t commandType);

    std::string_view AckStatusDescription(services::CanAckStatus status);
    std::string_view RejectionDescription(can::FocMotorCategoryError error);
}
