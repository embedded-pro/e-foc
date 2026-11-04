#pragma once

#include "integration_tests/support/interactor/interfaces/BootloaderTarget.hpp"
#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include "integration_tests/support/interactor/qemu/QemuSilSession.hpp"
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace sil
{
    struct QemuImages
    {
        std::string applicationElf;
        std::string bootloaderElf;
        std::string developmentKeySet;
    };

    class QemuInteractor
        : public integration::TargetInteractor
        , public integration::BootloaderTarget
    {
    public:
        explicit QemuInteractor(QemuImages images);

        void Setup() override;
        void Teardown() override;
        void BeforeScenario() override;
        void AfterScenario() override;

        bool SupportsSimulatedPlant() const override;
        void ConfigurePlant(const std::vector<uint8_t>& record) override;
        void ConfigureNonVolatileMemory(const std::vector<uint8_t>& image) override;
        void RestartTarget() override;

        void ClearCanFrames() override;
        bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
            std::chrono::milliseconds timeout) override;
        bool WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out,
            std::chrono::milliseconds timeout,
            std::chrono::steady_clock::time_point referenceTime,
            std::chrono::milliseconds& elapsed) override;
        std::size_t ReceivedCanFrameCount() const override;
        std::optional<hal::Can::Message> FindReceivedCanFrame(hal::Can::Id id, std::size_t fromFrame) const override;
        std::vector<hal::Can::Message> FindReceivedCanFrames(hal::Can::Id id, std::size_t fromFrame) const override;
        std::optional<hal::Can::Id> FindReceivedCanFrameId(const std::function<bool(hal::Can::Id)>& matches, std::size_t fromFrame) const override;

        std::vector<std::string> TakePlantRecord(std::chrono::milliseconds timeout) override;
        std::string DebugLog() const override;

        std::vector<uint8_t> ApplicationImage() const override;
        std::vector<uint8_t> DevelopmentKeySet() const override;
        void ConfigureSlot(Slot slot) override;
        void ConfigureStaging(const std::vector<uint8_t>& pack) override;
        void ConfigureJournal(const std::vector<uint8_t>& journal) override;
        std::vector<uint8_t> Journal() const override;

    private:
        QemuSilSession::Images LaunchImages();
        bool WriteWhileStopped(const std::string& name, const std::vector<uint8_t>& contents);

        QemuImages images;
        std::vector<uint8_t> bootloader;
        std::vector<uint8_t> applicationImage;
        std::vector<uint8_t> applicationDescriptor;
        std::vector<uint8_t> developmentKeySet;
        Slot slot{ Slot::application };
        QemuSilSession session;
    };
}
