#include "integration_tests/steps/UpgradePacks.hpp"
#include "application/bootloader_image/KeySet.hpp"
#include "bootloader/StatusJournal.hpp"
#include "generated/Boards.hpp"
#include "generated/Version.h"
#include "integration_tests/support/interactor/hardware/BridgeConfig.hpp"
#include "integration_tests/support/interactor/interfaces/Files.hpp"
#include "services/synchronous_flash/SynchronousFlashMemory.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
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

        std::string DevelopmentKeysPath()
        {
            const char* path = std::getenv("E_FOC_DEVELOPMENT_KEYS");
            return path != nullptr ? path : E_FOC_DEVELOPMENT_KEYS_DEFAULT_PATH;
        }

        PackSource HardwarePackSource()
        {
            auto image = std::filesystem::path{ hil::GetBridgeConfig().firmwareElf }.replace_extension(".image.bin");
            return PackSource{ RequireBinaryFile(image.string()), RequireBinaryFile(DevelopmentKeysPath()), e_foc::boards::ekTm4c1294xl.identification, applicationSlotStart, e_foc::boards::ekTm4c1294xl.kind };
        }

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
        if (auto* emulated = BootloaderTarget::Instance())
            return PackSource{ emulated->ApplicationImage(), emulated->DevelopmentKeySet(), e_foc::boards::emulated.identification, applicationSlotStart, e_foc::boards::emulated.kind };
        return HardwarePackSource();
    }

    std::string BuildVersion()
    {
        return std::to_string(e_foc::generated::VERSION_MAJOR) + "." + std::to_string(e_foc::generated::VERSION_MINOR) + "." + std::to_string(e_foc::generated::VERSION_PATCH);
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
