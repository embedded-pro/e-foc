#include "bootloader/StatusJournal.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "generated/Boards.hpp"
#include "integration_tests/steps/UpgradePacks.hpp"
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
#include <stdexcept>
#include <string>
#include <vector>

using namespace integration;

namespace
{
    constexpr std::size_t headerTargetOffset = 0x400 + 16;
    constexpr uint8_t anotherBoardKind = 1;

    application::PackDescription Description(const std::string& defect)
    {
        application::PackDescription description{ e_foc::boards::emulated.identification, "", "sync_foc_sensored", emulatedSlotStart, EmulatedBootloader().ApplicationImage() };

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
        auto pack = DevelopmentSignedPack(Description(defect));
        if (defect == "with a bad signature")
            pack.at(sizeof(application::UpgradePackHeaderPrologue)) ^= 0x01;
        return pack;
    }

    void Stage(const std::string& defect, const std::string& status)
    {
        EmulatedBootloader().ConfigureStaging(Pack(defect));
        EmulatedBootloader().ConfigureJournal(JournalReading(JournalStatusNamed(status)));
    }
}

GIVEN(R"(the application slot holds {string})", (std::string contents))
{
    if (contents == "the application")
        EmulatedBootloader().ConfigureSlot(BootloaderTarget::Slot::application);
    else if (contents == "a damaged application")
        EmulatedBootloader().ConfigureSlot(BootloaderTarget::Slot::damagedApplication);
    else if (contents == "nothing")
        EmulatedBootloader().ConfigureSlot(BootloaderTarget::Slot::empty);
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
    const auto [recorded, errorCode] = RecordedJournal();
    EXPECT_EQ(JournalStatusNamed(status), recorded) << "The journal reads " << static_cast<int>(recorded) << " with error code " << errorCode;
}

THEN(R"(the journal reads {string} with error code {int})", (std::string status, int expectedErrorCode))
{
    const auto [recorded, errorCode] = RecordedJournal();
    EXPECT_EQ(JournalStatusNamed(status), recorded) << "The journal reads " << static_cast<int>(recorded);
    EXPECT_EQ(static_cast<uint32_t>(expectedErrorCode), errorCode);
}
