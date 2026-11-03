#include "integration_tests/support/interactor/qemu/QemuInteractor.hpp"
#include "application/bootloader_image/ElfSection.hpp"
#include "application/bootloader_image/KeySet.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <utility>
#include <variant>

namespace sil
{
    namespace
    {
        constexpr uint32_t descriptorSize = 64;

        std::vector<uint8_t> ReadFile(const std::string& path)
        {
            std::ifstream file{ path, std::ios::binary };
            return { std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
        }

        std::string Sibling(const std::string& elfPath, const std::string& suffix)
        {
            const auto stem = elfPath.ends_with(".elf") ? elfPath.substr(0, elfPath.size() - 4) : elfPath;
            return stem + suffix;
        }

        bool InjectKeys(std::vector<uint8_t>& elf, const std::vector<uint8_t>& keySetFile)
        {
            const auto keys = application::ParseKeySet(keySetFile);
            const auto section = application::FindElfSection(elf, ".bootloader_keys");
            if (!std::holds_alternative<application::KeySet>(keys) || !std::holds_alternative<application::ElfSection>(section))
                return false;

            const auto slot = application::BootloaderKeySlot(std::get<application::KeySet>(keys));
            const auto& place = std::get<application::ElfSection>(section);
            if (place.size != slot.size())
                return false;
            std::ranges::copy(slot, elf.begin() + static_cast<std::ptrdiff_t>(place.fileOffset));
            return true;
        }
    }

    QemuInteractor::QemuInteractor(QemuImages images)
        : images{ std::move(images) }
    {}

    void QemuInteractor::Setup()
    {
        bootloader = ReadFile(images.bootloaderElf);
        applicationImage = ReadFile(Sibling(images.applicationElf, ".image.bin"));
        applicationDescriptor = ReadFile(Sibling(images.applicationElf, ".descriptor.bin"));
        developmentKeySet = ReadFile(images.developmentKeySet);

        ASSERT_FALSE(applicationImage.empty()) << "[QEMU] no application image beside " << images.applicationElf;
        ASSERT_EQ(descriptorSize, applicationDescriptor.size()) << "[QEMU] no image descriptor beside " << images.applicationElf;
        ASSERT_TRUE(InjectKeys(bootloader, developmentKeySet)) << "[QEMU] cannot write the development keys " << images.developmentKeySet
                                                               << " into the bootloader " << images.bootloaderElf;
    }

    void QemuInteractor::Teardown()
    {}

    void QemuInteractor::BeforeScenario()
    {
        slot = Slot::application;
        session.ResetCapture();
        if (!session.CreateScenarioDirectory())
            GTEST_FAIL() << "[QEMU] could not create the scenario working directory";
        if (!session.WriteScenarioFile("bootloader.elf", bootloader))
            GTEST_FAIL() << "[QEMU] could not write the bootloader";
        if (!session.Start(LaunchImages()))
            GTEST_FAIL() << "[QEMU] QEMU session failed to start; last debug output:\n"
                         << session.DebugLogTail();
    }

    void QemuInteractor::AfterScenario()
    {
        session.Stop();
        session.RemoveScenarioDirectory();
    }

    QemuSilSession::Images QemuInteractor::LaunchImages()
    {
        const auto directory = session.ScenarioDirectory();
        QemuSilSession::Images launch{ directory + "/bootloader.elf", std::nullopt };
        if (slot == Slot::empty)
            return launch;

        std::vector<uint8_t> contents(integration::emulatedSlotSize, 0xff);
        std::ranges::copy(applicationImage, contents.begin());
        std::ranges::copy(applicationDescriptor, contents.end() - descriptorSize);
        if (slot == Slot::damagedApplication)
            contents[applicationImage.size() / 2] ^= 0x01;

        if (session.WriteScenarioFile("slot.bin", contents))
            launch.slotImage = directory + "/slot.bin";
        return launch;
    }

    bool QemuInteractor::SupportsSimulatedPlant() const
    {
        return true;
    }

    void QemuInteractor::ConfigurePlant(const std::vector<uint8_t>& record)
    {
        ASSERT_TRUE(session.WriteScenarioFile("plant.bin", record))
            << "[QEMU] could not write the plant description";
    }

    void QemuInteractor::ConfigureNonVolatileMemory(const std::vector<uint8_t>& image)
    {
        ASSERT_TRUE(session.WriteScenarioFile("eeprom.bin", image))
            << "[QEMU] could not write the NVM image";
    }

    void QemuInteractor::RestartTarget()
    {
        ASSERT_TRUE(session.Restart(LaunchImages())) << "[QEMU] QEMU session failed to restart; last debug output:\n"
                                                     << session.DebugLogTail();
    }

    void QemuInteractor::ClearCanFrames()
    {}

    bool QemuInteractor::SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
        std::chrono::milliseconds timeout)
    {
        if (!session.IsRunning())
            return false;
        return session.SendCanFrame(id, message, timeout);
    }

    bool QemuInteractor::WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out,
        std::chrono::milliseconds timeout,
        std::chrono::steady_clock::time_point,
        std::chrono::milliseconds& elapsed)
    {
        if (!session.IsRunning())
            return false;
        const auto start = std::chrono::steady_clock::now();
        const bool found = session.WaitForCanFrame(expectedId, out, timeout);
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        return found;
    }

    std::size_t QemuInteractor::ReceivedCanFrameCount() const
    {
        return session.ReceivedFrames().size();
    }

    std::optional<hal::Can::Message> QemuInteractor::FindReceivedCanFrame(hal::Can::Id id, std::size_t fromFrame) const
    {
        const auto& frames = session.ReceivedFrames();
        std::optional<hal::Can::Message> found;

        for (std::size_t i = fromFrame; i < frames.size(); ++i)
            if (frames[i].id == id)
                found = frames[i].message;

        return found;
    }

    std::vector<hal::Can::Message> QemuInteractor::FindReceivedCanFrames(hal::Can::Id id, std::size_t fromFrame) const
    {
        const auto& frames = session.ReceivedFrames();
        std::vector<hal::Can::Message> found;

        for (std::size_t i = fromFrame; i < frames.size(); ++i)
            if (frames[i].id == id)
                found.push_back(frames[i].message);

        return found;
    }

    std::optional<hal::Can::Id> QemuInteractor::FindReceivedCanFrameId(const std::function<bool(hal::Can::Id)>& matches, std::size_t fromFrame) const
    {
        const auto& frames = session.ReceivedFrames();

        for (std::size_t i = fromFrame; i < frames.size(); ++i)
            if (matches(frames[i].id))
                return frames[i].id;

        return std::nullopt;
    }

    std::vector<std::string> QemuInteractor::TakePlantRecord(std::chrono::milliseconds timeout)
    {
        if (!session.IsRunning())
            return {};
        return session.TakePlantRecord(timeout);
    }

    std::string QemuInteractor::DebugLog() const
    {
        return session.DebugLogTail();
    }

    std::vector<uint8_t> QemuInteractor::ApplicationImage() const
    {
        return applicationImage;
    }

    std::vector<uint8_t> QemuInteractor::DevelopmentKeySet() const
    {
        return developmentKeySet;
    }

    void QemuInteractor::ConfigureSlot(Slot contents)
    {
        slot = contents;
    }

    void QemuInteractor::ConfigureStaging(const std::vector<uint8_t>& pack)
    {
        ASSERT_TRUE(WriteWhileStopped("staging.bin", pack)) << "[QEMU] could not stage the pack";
    }

    void QemuInteractor::ConfigureJournal(const std::vector<uint8_t>& journal)
    {
        ASSERT_TRUE(WriteWhileStopped("journal.bin", journal)) << "[QEMU] could not write the status journal";
    }

    bool QemuInteractor::WriteWhileStopped(const std::string& name, const std::vector<uint8_t>& contents)
    {
        session.Stop();
        return session.WriteScenarioFile(name, contents);
    }

    std::vector<uint8_t> QemuInteractor::Journal() const
    {
        auto journal = ReadFile(session.ScenarioDirectory() + "/journal.bin");
        journal.resize(integration::emulatedJournalSize, 0xff);
        return journal;
    }
}
