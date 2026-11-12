#pragma once

#include "infra/util/ByteRange.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace virtual_motor
{
    std::optional<std::size_t> SendWithoutWaiting(intptr_t socket, infra::ConstByteRange bytes);
}
