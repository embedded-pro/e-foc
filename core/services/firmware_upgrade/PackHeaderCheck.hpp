#pragma once

#include "core/services/firmware_upgrade/FirmwareVersion.hpp"
#include "infra/util/BoundedString.hpp"
#include "upgrade/pack/UpgradePackHeader.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace services
{
    struct PackHeader
    {
        application::UpgradePackHeaderPrologue prologue;
        application::UpgradePackHeaderEpilogue epilogue;
        application::ImageHeaderPrologue image;
    };

    struct PackLimits
    {
        infra::BoundedConstString productName;
        uint32_t applicationCapacity;
        uint32_t packSize;
    };

    constexpr uint32_t minimumPackSize = sizeof(application::UpgradePackHeaderPrologue) + sizeof(application::UpgradePackHeaderEpilogue);

    uint32_t EpilogueOffset(const application::UpgradePackHeaderPrologue& prologue);
    std::optional<FirmwareVersion> ParseVersion(const std::array<char, 64>& text);
    std::optional<FirmwareVersion> CheckPackHeader(const PackHeader& header, const PackLimits& limits);
}
