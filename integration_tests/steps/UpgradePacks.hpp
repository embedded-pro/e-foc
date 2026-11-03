#pragma once

#include "application/bootloader_image/Pack.hpp"
#include "integration_tests/support/interactor/interfaces/BootloaderTarget.hpp"
#include "upgrade/pack/UpgradePackHeader.hpp"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace integration
{
    struct PackSource
    {
        std::vector<uint8_t> applicationImage;
        std::vector<uint8_t> developmentKeySet;
        std::string productName;
        uint32_t slotStart;
        uint8_t boardKind;
    };

    BootloaderTarget& EmulatedBootloader();
    PackSource TargetPackSource();
    std::vector<uint8_t> DevelopmentSignedPack(const application::PackDescription& description);
    application::UpgradePackStatus JournalStatusNamed(const std::string& name);
    std::vector<uint8_t> JournalReading(application::UpgradePackStatus status);
    std::pair<application::UpgradePackStatus, uint32_t> RecordedJournal();
}
