#pragma once

#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include "integration_tests/support/interactor/qemu/QemuSilSession.hpp"
#include <chrono>
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

        bool SendCommand(const std::string& command, std::chrono::milliseconds timeout) override;
        bool DrainSerial(std::chrono::milliseconds timeout) override;
        bool WaitForPrompt(std::chrono::milliseconds timeout) override;
        void ClearSerialLines() override;
        const std::vector<std::string>& SerialLines() const override;
        const std::string& LastSerialLine() const override;
        std::chrono::milliseconds LastSerialDuration() const override;

        void ClearCanFrames() override;
        bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
            std::chrono::milliseconds timeout) override;
        bool WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out,
            std::chrono::milliseconds timeout,
            std::chrono::steady_clock::time_point referenceTime,
            std::chrono::milliseconds& elapsed) override;

    private:
        std::string elfPath;
        QemuSilSession session;
        std::vector<std::string> lines;
        std::string lastLine;
    };
}
