#pragma once

#include "tools/virtual_motor/app/logic/CaptureAssembly.hpp"
#include <cstdint>
#include <string>

namespace virtual_motor::app
{
    double CaptureTime(const Capture& capture, std::size_t index, uint32_t controlRateHz);
    std::string CaptureCsv(const Capture& capture, uint32_t controlRateHz);
}
