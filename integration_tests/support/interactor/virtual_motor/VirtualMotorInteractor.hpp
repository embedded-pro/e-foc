#pragma once

#include "can-lite/drivers/codec/SlcanCodec.hpp"
#include "can-lite/drivers/implementation/VirtualBusPort.hpp"
#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include <chrono>
#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

namespace virtual_motor
{
    class VirtualMotorInteractor
        : public integration::TargetInteractor
    {
    public:
        explicit VirtualMotorInteractor(std::string program);
        ~VirtualMotorInteractor() override;

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

        std::string DebugLog() const override;

    private:
        struct Frame
        {
            hal::Can::Id id;
            hal::Can::Message message;
        };

        bool CreateDirectory();
        void RemoveDirectory();
        bool WriteFile(const std::string& name, const std::vector<uint8_t>& contents) const;
        bool Start();
        void Stop();
        bool Spawn(uint16_t hubPort);
        [[noreturn]] void Exec(uint16_t hubPort, int outputFd) const;
        bool JoinBus(uint16_t hubPort);
        bool PumpUntil(std::chrono::steady_clock::time_point deadline, const std::function<bool()>& done);
        void Pump(std::chrono::milliseconds timeout);
        void ReadOutput();
        void ReadBus();
        void OnOutputLine(std::string line);
        bool TakeFrame(hal::Can::Id expectedId, hal::Can::Message& out);

        static constexpr std::size_t debugTailLines{ 200 };

        std::string program;
        std::string directory;
        bool plantDescribed{ false };
        pid_t pid{ -1 };
        int outputFd{ -1 };
        std::string outputPartial;
        std::deque<std::string> debugTail;
        std::ofstream debugLog;
        std::optional<services::VirtualBusPort> bus;
        services::SlcanDecoder decoder;
        std::vector<Frame> received;
        std::size_t nextFrame{ 0 };
    };
}
