#include "integration_tests/steps/UpgradePacks.hpp"
#include "application/bootloader_image/KeySet.hpp"
#include "bootloader/StatusJournal.hpp"
#include "services/synchronous_flash/SynchronousFlashMemory.hpp"
#include <algorithm>
#include <map>
#include <random>
#include <span>
#include <stdexcept>
#include <variant>

namespace integration
{
    namespace
    {
        const std::map<std::string, application::UpgradePackStatus> statuses{
            { "empty", application::UpgradePackStatus::empty },
            { "downloaded", application::UpgradePackStatus::downloaded },
            { "ready to deploy", application::UpgradePackStatus::readyToDeploy },
            { "deploy started", application::UpgradePackStatus::deployStarted },
            { "deployed", application::UpgradePackStatus::deployed },
            { "invalid", application::UpgradePackStatus::invalid },
        };

        bool Random(std::span<uint8_t> bytes)
        {
            static std::random_device device;
            std::ranges::generate(bytes, []()
                {
                    return static_cast<uint8_t>(device());
                });
            return true;
        }
    }

    BootloaderTarget& EmulatedBootloader()
    {
        auto* target = BootloaderTarget::Instance();
        if (target == nullptr)
            throw std::runtime_error{ "Only the emulated target boots a bootloader the harness can stage packs for" };
        return *target;
    }

    PackSource TargetPackSource()
    {
        auto& emulated = EmulatedBootloader();
        return PackSource{ emulated.ApplicationImage(), emulated.DevelopmentKeySet(), E_FOC_EMULATED_PRODUCT_NAME, emulatedSlotStart, E_FOC_EMULATED_BOARD_KIND };
    }

    std::vector<uint8_t> DevelopmentSignedPack(const application::PackDescription& description)
    {
        const auto keys = application::ParseKeySet(TargetPackSource().developmentKeySet);
        if (!std::holds_alternative<application::KeySet>(keys))
            throw std::runtime_error{ "The development key set is not a key set" };

        auto built = application::BuildPack(description, std::get<application::KeySet>(keys), Random);
        if (!std::holds_alternative<std::vector<uint8_t>>(built))
            throw std::runtime_error{ "The pack could not be built" };

        return std::get<std::vector<uint8_t>>(built);
    }

    application::UpgradePackStatus JournalStatusNamed(const std::string& name)
    {
        const auto status = statuses.find(name);
        if (status == statuses.end())
            throw std::runtime_error{ "Unknown journal status: " + name };
        return status->second;
    }

    std::vector<uint8_t> JournalReading(application::UpgradePackStatus status)
    {
        std::vector<uint8_t> memory(emulatedJournalSize, 0xff);
        services::SynchronousFlashMemory flash{ infra::MakeRange(memory), emulatedJournalSize };
        bootloader::StatusJournal{ flash }.Record(status, 0);
        return memory;
    }

    std::pair<application::UpgradePackStatus, uint32_t> RecordedJournal()
    {
        auto memory = EmulatedBootloader().Journal();
        services::SynchronousFlashMemory flash{ infra::MakeRange(memory), emulatedJournalSize };
        const bootloader::StatusJournal journal{ flash };
        return { journal.Status(), journal.ErrorCode() };
    }
}
