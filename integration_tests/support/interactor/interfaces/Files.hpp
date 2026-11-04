#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace integration
{
    std::optional<std::vector<uint8_t>> ReadBinaryFile(const std::string& path);
    std::vector<uint8_t> RequireBinaryFile(const std::string& path);
}
