#pragma once

#include "hal/interfaces/Can.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace integration
{
    class TargetInteractor
    {
    public:
        virtual ~TargetInteractor() = default;

        virtual void Setup() = 0;
        virtual void Teardown() = 0;
        virtual void BeforeScenario() = 0;
        virtual void AfterScenario() = 0;

        virtual bool SupportsSimulatedPlant() const;
        virtual void ConfigurePlant(const std::vector<uint8_t>& record);
        virtual void ConfigureNonVolatileMemory(const std::vector<uint8_t>& image);
        virtual void RestartTarget();

        virtual void ClearCanFrames() = 0;
        virtual bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
            std::chrono::milliseconds timeout) = 0;
        virtual bool WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out,
            std::chrono::milliseconds timeout,
            std::chrono::steady_clock::time_point referenceTime,
            std::chrono::milliseconds& elapsed) = 0;

        // A wait for one frame drops the others it reads; these keep every frame received from the target.
        virtual std::size_t ReceivedCanFrameCount() const;
        virtual std::optional<hal::Can::Message> FindReceivedCanFrame(hal::Can::Id id, std::size_t fromFrame) const;
        virtual std::vector<hal::Can::Message> FindReceivedCanFrames(hal::Can::Id id, std::size_t fromFrame) const;
        virtual std::optional<hal::Can::Id> FindReceivedCanFrameId(const std::function<bool(hal::Can::Id)>& matches, std::size_t fromFrame) const;

        virtual std::vector<std::string> TakePlantRecord(std::chrono::milliseconds timeout);
        virtual std::string DebugLog() const;

        static TargetInteractor& Instance();
        static void SetInstance(TargetInteractor& instance);
    };
}
