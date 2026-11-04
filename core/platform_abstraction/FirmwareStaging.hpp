#pragma once

#include "hal/interfaces/Flash.hpp"
#include "hal/synchronous_interfaces/SynchronousFlash.hpp"
#include "infra/util/BoundedString.hpp"
#include <cstdint>

namespace application
{
    struct FirmwareStaging
    {
        hal::Flash& pack;
        hal::SynchronousFlash& journal;
        infra::BoundedConstString productName;
        uint32_t applicationCapacity;
        uint16_t pageSize;
    };
}
