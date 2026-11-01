#pragma once

#include "core/bootloader/ImageFormat.hpp"
#include "infra/util/ByteRange.hpp"

namespace bootloader
{
    bool ImageStartFitsTarget(infra::ConstByteRange imageStart, const TargetLayout& layout, const TargetIdentity& identity);
    bool ApplicationIsStartable(infra::ConstByteRange slot, const TargetLayout& layout, const TargetIdentity& identity);
    ImageDescriptor MakeImageDescriptor(infra::ConstByteRange image);
}
