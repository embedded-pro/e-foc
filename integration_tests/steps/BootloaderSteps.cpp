#include "application/bootloader_image/KeySet.hpp"
#include "application/bootloader_image/Pack.hpp"
#include "bootloader/StatusJournal.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include "integration_tests/support/interactor/interfaces/BootloaderTarget.hpp"
#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include "integration_tests/support/nvm/NvmImageBuilder.hpp"
#include "integration_tests/support/plant/PlantConfigWriter.hpp"
#include "services/synchronous_flash/SynchronousFlashMemory.hpp"
#include "upgrade/pack/UpgradePackHeader.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <map>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

using namespace integration;

namespace
{
    constexpr std::size_t headerTargetOffset = 0x400 + 16;
    constexpr uint8_t anotherBoardKind = 1;
    constexpr application::InitialCounter counter{ 0x5a, 0x3c, 0x96, 0x0f, 0xa5, 0xc3, 0x69, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf1 };

    const std::map<std::string, application::UpgradePackStatus> statuses{
        { "empty", application::UpgradePackStatus::empty },
        { "downloaded", application::UpgradePackStatus::downloaded },
        { "ready to deploy", application::UpgradePackStatus::readyToDeploy },
        { "deploy started", application::UpgradePackStatus::deployStarted },
        { "deployed", application::UpgradePackStatus::deployed },
        { "invalid", application::UpgradePackStatus::invalid },
    };

    BootloaderTarget& Target()
    {
        auto* target = BootloaderTarget::Instance();
        if (target == nullptr)
            throw std::runtime_error{ "Only the emulated target boots a bootloader the harness can stage packs for" };
        return *target;
    }

    application::UpgradePackStatus Status(const std::string& name)
    {
        const auto status = statuses.find(name);
        if (status == statuses.end())
            throw std::runtime_error{ "Unknown journal status: " + name };
        return status->second;
    }

    application::PackDescription Description(const std::string& defect)
    {
        application::PackDescription description{ E_FOC_EMULATED_PRODUCT_NAME, "", "sync_foc_sensored", emulatedSlotStart, Target().ApplicationImage() };

        if (defect == "for another product")
            description.productName = "e-foc another product";
        else if (defect == "for another board")
            description.image.at(headerTargetOffset) = anotherBoardKind;
        else if (defect == "for another destination")
            description.destination = 0;
        else if (!defect.empty() && defect != "with a bad signature")
            throw std::runtime_error{ "Unknown pack defect: " + defect };
        return description;
    }

    std::vector<uint8_t> Pack(const std::string& defect)
    {
        const auto keys = application::ParseKeySet(Target().DevelopmentKeySet());
        if (!std::holds_alternative<application::KeySet>(keys))
            throw std::runtime_error{ "The development key set is not a key set" };

        auto built = application::BuildPack(Description(defect), std::get<application::KeySet>(keys), counter);
        if (!std::holds_alternative<std::vector<uint8_t>>(built))
            throw std::runtime_error{ "The pack could not be built" };

        auto pack = std::get<std::vector<uint8_t>>(built);
        if (defect == "with a bad signature")
            pack.at(sizeof(application::UpgradePackHeaderPrologue)) ^= 0x01;
        return pack;
    }

    std::vector<uint8_t> JournalReading(application::UpgradePackStatus status)
    {
        std::vector<uint8_t> memory(emulatedJournalSize, 0xff);
        services::SynchronousFlashMemory flash{ infra::MakeRange(memory), emulatedJournalSize };
        bootloader::StatusJournal{ flash }.Record(status, 0);
        return memory;
    }

    std::pair<application::UpgradePackStatus, uint32_t> ReadJournal()
    {
        auto memory = Target().Journal();
        services::SynchronousFlashMemory flash{ infra::MakeRange(memory), emulatedJournalSize };
        const bootloader::StatusJournal journal{ flash };
        return { journal.Status(), journal.ErrorCode() };
    }

    void Stage(const std::string& defect, const std::string& status)
    {
        Target().ConfigureStaging(Pack(defect));
        Target().ConfigureJournal(JournalReading(Status(status)));
    }
}

GIVEN(R"(the application slot holds {string})", (std::string contents))
{
    if (contents == "the application")
        Target().ConfigureSlot(BootloaderTarget::Slot::application);
    else if (contents == "a damaged application")
        Target().ConfigureSlot(BootloaderTarget::Slot::damagedApplication);
    else if (contents == "nothing")
        Target().ConfigureSlot(BootloaderTarget::Slot::empty);
    else
        FAIL() << "Unknown slot contents: " << contents;
}

GIVEN(R"(a development-signed pack of the application is staged and the journal reads {string})", (std::string status))
{
    Stage("", status);
}

GIVEN(R"(a development-signed pack {string} is staged and the journal reads {string})", (std::string defect, std::string status))
{
    Stage(defect, status);
}

WHEN(R"(the emulated target starts)")
{
    auto& setup = context.Get<ScenarioSetup>();
    auto& interactor = TargetInteractor::Instance();

    interactor.ConfigurePlant(sil::EncodePlantConfig(setup.plant));
    interactor.ConfigureNonVolatileMemory(sil::BlankNvmImage());
    interactor.RestartTarget();
    setup.booted = true;
}

THEN(R"(the application sends its heartbeat)")
{
    EXPECT_TRUE(context.Get<Fixture>().WaitForCanHeartbeat()) << "No application answered after the bootloader ran";
}

THEN(R"(no heartbeat arrives within {int} s)", (int seconds))
{
    EXPECT_FALSE(context.Get<Fixture>().WaitForCanHeartbeat(std::chrono::seconds{ seconds })) << "An application started";
}

THEN(R"(the debug log says {string})", (std::string text))
{
    EXPECT_NE(TargetInteractor::Instance().DebugLog().find(text), std::string::npos) << "The debug log does not say: " << text;
}

THEN(R"(the journal reads {string})", (std::string status))
{
    const auto [recorded, errorCode] = ReadJournal();
    EXPECT_EQ(Status(status), recorded) << "The journal reads " << static_cast<int>(recorded) << " with error code " << errorCode;
}

THEN(R"(the journal reads {string} with error code {int})", (std::string status, int expectedErrorCode))
{
    const auto [recorded, errorCode] = ReadJournal();
    EXPECT_EQ(Status(status), recorded) << "The journal reads " << static_cast<int>(recorded);
    EXPECT_EQ(static_cast<uint32_t>(expectedErrorCode), errorCode);
}
