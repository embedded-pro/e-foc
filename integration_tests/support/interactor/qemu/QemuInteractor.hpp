#pragma once

#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include "integration_tests/support/interactor/qemu/QemuSilSession.hpp"
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace sil
{
    class QemuInteractor : public integration::TargetInteractor
    {
    public:
        explicit QemuInteractor(std::string elfPath);

        void Setup() override;
        void Teardown() override;
        void BeforeScenario() override;
        void AfterScenario() override;

        bool SupportsSimulatedPlant() const override;
        void ConfigurePlant(const std::vector<uint8_t>& record) override;
        void ConfigureNonVolatileMemory(const std::vector<uint8_t>& image) override;
        void RestartTarget() override;

        bool SendCommand(const std::string& command, std::chrono::milliseconds timeout) override;
        bool DrainSerial(std::chrono::milliseconds timeout) override;
        bool WaitForPrompt(std::chrono::milliseconds timeout) override;
        void ClearSerialLines() override;
        const std::vector<std::string>& SerialLines() const override;
        std::size_t DiscardedSerialLineCount() const override;
        const std::string& LastSerialLine() const override;
        std::chrono::milliseconds LastSerialDuration() const override;

        void ClearCanFrames() override;
        bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
            std::chrono::milliseconds timeout) override;
        bool WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out,
            std::chrono::milliseconds timeout,
            std::chrono::steady_clock::time_point referenceTime,
            std::chrono::milliseconds& elapsed) override;
        std::size_t ReceivedCanFrameCount() const override;
        std::optional<hal::Can::Message> FindReceivedCanFrame(hal::Can::Id id, std::size_t fromFrame) const override;
        std::optional<hal::Can::Id> FindReceivedCanFrameId(const std::function<bool(hal::Can::Id)>& matches, std::size_t fromFrame) const override;

        std::vector<std::string> TakePlantRecord(std::chrono::milliseconds timeout) override;
        std::string DebugLog() const override;

    private:
        std::string elfPath;
        QemuSilSession session;
        std::string lastLine;
        std::size_t discardedSerialLines{ 0 };
    };
}
