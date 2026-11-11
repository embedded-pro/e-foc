#include "bootloader/StatusJournal.hpp"
#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeDefinitions.hpp"
#include "can-lite/core/CanPayload.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "generated/Boards.hpp"
#include "generated/Version.h"
#include "integration_tests/steps/UpgradePacks.hpp"
#include "integration_tests/support/CanParameters.hpp"
#include "integration_tests/support/FirmwareUpgradeClient.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <gtest/gtest.h>
#include <map>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace integration;

namespace
{
    constexpr std::size_t smallImageSize = 600;
    constexpr std::size_t staleImageSize = 4096;
    constexpr auto installTimeout = std::chrono::seconds{ 60 };
    constexpr auto activateToDeployedBound = std::chrono::duration<double>{ 5.0 };
    constexpr auto beginAnswerBound = std::chrono::duration<double>{ 1.0 };
    constexpr auto silenceTimeout = std::chrono::seconds{ 10 };
    constexpr auto silence = std::chrono::milliseconds{ 1500 };
    constexpr auto journalPollInterval = std::chrono::milliseconds{ 100 };

    const std::map<std::string, services::FwuError> answers{
        { "ok", services::FwuError::ok },
        { "busy", services::FwuError::busy },
        { "CRC mismatch", services::FwuError::crcMismatch },
        { "not ready", services::FwuError::notReady },
        { "invalid state", services::FwuError::invalidState },
    };

    uint8_t AnswerNamed(const std::string& name)
    {
        const auto answer = answers.find(name);
        if (answer == answers.end())
            throw std::runtime_error{ "Unknown firmware-upgrade answer: " + name };
        return static_cast<uint8_t>(answer->second);
    }

    std::vector<uint8_t> Image(std::size_t size)
    {
        std::vector<uint8_t> image(size);
        for (std::size_t i = 0; i != image.size(); ++i)
            image[i] = static_cast<uint8_t>(i * 13 + 7);
        return image;
    }

    std::vector<uint8_t> SmallImage()
    {
        return Image(smallImageSize);
    }

    bool ErasedFrom(const std::vector<uint8_t>& memory, std::size_t offset)
    {
        return std::all_of(memory.begin() + static_cast<std::ptrdiff_t>(offset), memory.end(), [](uint8_t byte)
            {
                return byte == 0xff;
            });
    }

    application::PackDescription PackOf(const std::string& version, std::vector<uint8_t> image)
    {
        const auto source = TargetPackSource();
        return application::PackDescription{ source.productName, version, "sync_foc_sensored", source.slotStart, std::move(image) };
    }

    application::PackDescription ApplicationPack()
    {
        return PackOf(BuildVersion(), TargetPackSource().applicationImage);
    }

    void Prepare(FirmwareDownload& download, const application::PackDescription& description)
    {
        download.pack = DevelopmentSignedPack(description);
        download.crc = FirmwareUpgradeClient::Crc(download.pack);
        download.blocksSent = 0;
    }

    void Begin(Fixture& fixture, FirmwareDownload& download)
    {
        const auto sent = std::chrono::steady_clock::now();
        const auto answer = FirmwareUpgradeClient{ fixture }.Begin(static_cast<uint32_t>(download.pack.size()));
        const auto answered = std::chrono::duration<double>{ std::chrono::steady_clock::now() - sent }.count();
        ASSERT_TRUE(answer.has_value()) << "Begin Upgrade was not answered";
        std::printf("[METRIC] firmware upgrade begin_to_answer_s=%.2f\n", answered);
        EXPECT_LE(answered, beginAnswerBound.count()) << "Begin Upgrade kept the controller silent for too long";
        download.beginAnswer = static_cast<uint8_t>(answer->error);
        download.pageSize = answer->pageSize;
    }

    void SendBlocks(Fixture& fixture, FirmwareDownload& download, uint16_t count)
    {
        const auto result = FirmwareUpgradeClient{ fixture }.Blocks(download.pack, download.blocksSent, count);
        ASSERT_EQ(services::FwuError::ok, result) << "A block from " << download.blocksSent << " was not acknowledged";
        download.blocksSent += count;
    }

    void Download(Fixture& fixture, FirmwareDownload& download)
    {
        Begin(fixture, download);
        ASSERT_EQ(AnswerNamed("ok"), download.beginAnswer) << "Begin Upgrade was refused";
        SendBlocks(fixture, download, static_cast<uint16_t>(FirmwareUpgradeClient::NumberOfBlocks(download.pack) - download.blocksSent));

        const auto verified = FirmwareUpgradeClient{ fixture }.Verify(download.crc);
        ASSERT_TRUE(verified.has_value()) << "Verify was not answered";
        download.verifyAnswer = static_cast<uint8_t>(*verified);
    }

    void ExpectParameter(Fixture& fixture, can::FocParameter parameter, uint32_t expected)
    {
        const auto value = ReadParameterValue(fixture, parameter);
        ASSERT_TRUE(value.has_value()) << "The drive did not answer parameter " << static_cast<int>(parameter);
        EXPECT_EQ(static_cast<float>(expected), *value) << "parameter " << static_cast<int>(parameter);
    }

    std::vector<uint32_t> VersionNumbers(const std::string& version)
    {
        std::vector<uint32_t> numbers;
        std::size_t start = 0;
        while (start <= version.size())
        {
            const auto end = std::min(version.find('.', start), version.size());
            numbers.push_back(static_cast<uint32_t>(std::stoul(version.substr(start, end - start))));
            start = end + 1;
        }
        if (numbers.size() != 3)
            throw std::runtime_error{ "A version has three numbers: " + version };
        return numbers;
    }
}

WHEN(R"(a pack of version {string} is downloaded)", (std::string version))
{
    auto& download = context.Get<ScenarioSetup>().download;
    Prepare(download, PackOf(version, SmallImage()));
    Download(context.Get<Fixture>(), download);
}

WHEN(R"(a pack of version {string} for another board is downloaded)", (std::string version))
{
    auto& download = context.Get<ScenarioSetup>().download;
    auto description = PackOf(version, SmallImage());
    description.productName = e_foc::boards::ekTm4c1294xl.identification;
    Prepare(download, description);
    Download(context.Get<Fixture>(), download);
}

WHEN(R"(a pack of version {string} is downloaded with a CRC that does not match)", (std::string version))
{
    auto& download = context.Get<ScenarioSetup>().download;
    Prepare(download, PackOf(version, SmallImage()));
    download.crc ^= 1;
    Download(context.Get<Fixture>(), download);
}

GIVEN(R"(the application is staged as a downloaded pack of the version it runs)")
{
    auto& download = context.Get<ScenarioSetup>().download;
    Prepare(download, ApplicationPack());
    EmulatedBootloader().ConfigureStaging(download.pack);
    EmulatedBootloader().ConfigureJournal(JournalReading(application::UpgradePackStatus::downloaded));
}

WHEN(R"(the application is downloaded as a pack of the version it runs)")
{
    auto& download = context.Get<ScenarioSetup>().download;
    Prepare(download, ApplicationPack());
    Download(context.Get<Fixture>(), download);
}

WHEN(R"(Begin Upgrade is sent for a pack of version {string})", (std::string version))
{
    auto& download = context.Get<ScenarioSetup>().download;
    Prepare(download, PackOf(version, SmallImage()));
    Begin(context.Get<Fixture>(), download);
}

WHEN(R"(the first {int} blocks are sent)", (int count))
{
    auto& download = context.Get<ScenarioSetup>().download;
    ASSERT_EQ(AnswerNamed("ok"), download.beginAnswer) << "Begin Upgrade was refused";
    SendBlocks(context.Get<Fixture>(), download, static_cast<uint16_t>(count));
}

WHEN(R"(Activate is sent)")
{
    auto& download = context.Get<ScenarioSetup>().download;
    const auto answer = FirmwareUpgradeClient{ context.Get<Fixture>() }.Activate();
    download.activatedAt = std::chrono::steady_clock::now();
    ASSERT_TRUE(answer.has_value()) << "Activate was not answered";
    download.activateAnswer = static_cast<uint8_t>(*answer);
}

GIVEN(R"(the staging slot holds a stale pack and the journal reads {string})", (std::string status))
{
    auto& download = context.Get<ScenarioSetup>().download;
    download.stale = DevelopmentSignedPack(PackOf("9.9.9", Image(staleImageSize)));
    EmulatedBootloader().ConfigureStaging(download.stale);
    EmulatedBootloader().ConfigureJournal(JournalReading(JournalStatusNamed(status)));
}

WHEN(R"(Abort is sent)")
{
    ASSERT_TRUE(FirmwareUpgradeClient{ context.Get<Fixture>() }.Abort()) << "Abort was not acknowledged";
}

WHEN(R"(the emergency stop is sent)")
{
    ASSERT_TRUE(context.Get<Fixture>().SendCanCommand(can::focMotorCategoryId, can::focEmergencyStopId)) << "The emergency stop was refused";
}

THEN(R"(Begin Upgrade is answered {string})", (std::string answer))
{
    EXPECT_EQ(AnswerNamed(answer), context.Get<ScenarioSetup>().download.beginAnswer);
}

THEN(R"(Begin Upgrade was answered with a page size of {int})", (int pageSize))
{
    EXPECT_EQ(static_cast<uint16_t>(pageSize), context.Get<ScenarioSetup>().download.pageSize);
}

THEN(R"(the download is answered {string})", (std::string answer))
{
    EXPECT_EQ(AnswerNamed(answer), context.Get<ScenarioSetup>().download.verifyAnswer);
}

THEN(R"(Activate is answered {string})", (std::string answer))
{
    EXPECT_EQ(AnswerNamed(answer), context.Get<ScenarioSetup>().download.activateAnswer);
}

THEN(R"(the next block is answered {string})", (std::string answer))
{
    auto& download = context.Get<ScenarioSetup>().download;
    const auto result = FirmwareUpgradeClient{ context.Get<Fixture>() }.Block(download.pack, download.blocksSent);
    ASSERT_TRUE(result.has_value()) << "The block was not answered";
    EXPECT_EQ(AnswerNamed(answer), static_cast<uint8_t>(*result));
}

THEN(R"(the staging slot holds the pack and is erased past it)")
{
    const auto& pack = context.Get<ScenarioSetup>().download.pack;
    const auto staging = EmulatedBootloader().Staging();

    EXPECT_TRUE(std::ranges::equal(pack, staging | std::views::take(pack.size()))) << "The staging slot does not hold the pack";
    EXPECT_TRUE(ErasedFrom(staging, pack.size())) << "The staging slot is not erased past the pack";
}

THEN(R"(the staging slot still holds the stale pack)")
{
    const auto& stale = context.Get<ScenarioSetup>().download.stale;
    const auto staging = EmulatedBootloader().Staging();

    ASSERT_FALSE(stale.empty()) << "No stale pack was staged";
    EXPECT_TRUE(std::ranges::equal(stale, staging | std::views::take(stale.size()))) << "The stale pack was touched";
}

THEN(R"(the journal holds one entry)")
{
    const auto journal = EmulatedBootloader().Journal();
    EXPECT_TRUE(ErasedFrom(journal, sizeof(bootloader::StatusJournalEntry))) << "The journal holds more than one entry";
}

THEN(R"(the drive reports the staged version {string})", (std::string version))
{
    auto& fixture = context.Get<Fixture>();
    const auto numbers = VersionNumbers(version);
    ExpectParameter(fixture, can::FocParameter::stagedVersionMajor, numbers[0]);
    ExpectParameter(fixture, can::FocParameter::stagedVersionMinor, numbers[1]);
    ExpectParameter(fixture, can::FocParameter::stagedVersionPatch, numbers[2]);
}

THEN(R"(the drive reports the staged version of its build)")
{
    auto& fixture = context.Get<Fixture>();
    ExpectParameter(fixture, can::FocParameter::stagedVersionMajor, e_foc::generated::VERSION_MAJOR);
    ExpectParameter(fixture, can::FocParameter::stagedVersionMinor, e_foc::generated::VERSION_MINOR);
    ExpectParameter(fixture, can::FocParameter::stagedVersionPatch, e_foc::generated::VERSION_PATCH);
}

THEN(R"(the drive reports the version of its build and its board kind)")
{
    auto& fixture = context.Get<Fixture>();
    ExpectParameter(fixture, can::FocParameter::firmwareVersionMajor, e_foc::generated::VERSION_MAJOR);
    ExpectParameter(fixture, can::FocParameter::firmwareVersionMinor, e_foc::generated::VERSION_MINOR);
    ExpectParameter(fixture, can::FocParameter::firmwareVersionPatch, e_foc::generated::VERSION_PATCH);
    ExpectParameter(fixture, can::FocParameter::boardKind, TargetPackSource().boardKind);
}

THEN(R"(setting the telemetry rate to {int} Hz is refused as busy)", (int rateHz))
{
    auto& fixture = context.Get<Fixture>();

    const auto mark = fixture.MarkCapture();
    EXPECT_FALSE(WriteParameter(fixture, can::FocParameter::telemetryRate, static_cast<float>(rateHz))) << "The telemetry rate was accepted";

    const auto error = CapturedResponse(fixture, services::canCategoryErrorResponseMessageTypeId, mark);
    ASSERT_TRUE(error.has_value() && error->size() == 2) << "The drive sent no category error";
    EXPECT_EQ((*error)[0], can::focSetParameterId);
    EXPECT_EQ((*error)[1], static_cast<uint8_t>(can::FocMotorCategoryError::busy));
}

THEN(R"(the bootloader installs the pack and the application starts again)")
{
    const auto& download = context.Get<ScenarioSetup>().download;
    ASSERT_TRUE(download.activatedAt.has_value()) << "Activate was not sent";
    const auto deadline = *download.activatedAt + installTimeout;
    while (RecordedJournal().first != application::UpgradePackStatus::deployed && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(journalPollInterval);
    const auto installed = std::chrono::duration<double>{ std::chrono::steady_clock::now() - *download.activatedAt }.count();

    ASSERT_EQ(application::UpgradePackStatus::deployed, RecordedJournal().first) << "The bootloader did not record the pack deployed";
    std::printf("[METRIC] firmware upgrade activate_to_deployed_s=%.2f\n", installed);
    EXPECT_LE(installed, activateToDeployedBound.count()) << "The restart and the installation took too long after Activate";
    EXPECT_TRUE(context.Get<Fixture>().WaitForCanHeartbeat()) << "No application answered after the installation";
}

THEN(R"(the drive falls silent and its heartbeat comes back)")
{
    auto& fixture = context.Get<Fixture>();
    const auto& download = context.Get<ScenarioSetup>().download;
    ASSERT_TRUE(download.activatedAt.has_value()) << "Activate was not sent";
    const auto deadline = std::chrono::steady_clock::now() + silenceTimeout;
    bool fellSilent = false;
    while (!fellSilent && std::chrono::steady_clock::now() < deadline)
        fellSilent = !fixture.WaitForCanHeartbeat(silence);

    ASSERT_TRUE(fellSilent) << "The drive kept sending its heartbeat after Activate";
    const bool back = fixture.WaitForCanHeartbeat(installTimeout);
    const auto elapsed = std::chrono::duration<double>{ std::chrono::steady_clock::now() - *download.activatedAt }.count();
    std::printf("[METRIC] firmware upgrade activate_to_heartbeat_s=%.2f\n", elapsed);
    EXPECT_TRUE(back) << "No application answered after the installation";
}
