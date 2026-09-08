#pragma once

#include "hal/interfaces/Can.hpp"
#include <chrono>
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

        virtual bool SendCommand(const std::string& command, std::chrono::milliseconds timeout) = 0;
        virtual bool DrainSerial(std::chrono::milliseconds timeout) = 0;
        virtual bool WaitForPrompt(std::chrono::milliseconds timeout) = 0;
        virtual void ClearSerialLines() = 0;
        virtual const std::vector<std::string>& SerialLines() const = 0;
        virtual const std::string& LastSerialLine() const = 0;
        virtual std::chrono::milliseconds LastSerialDuration() const = 0;

        virtual void ClearCanFrames() = 0;
        virtual bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
            std::chrono::milliseconds timeout) = 0;
        virtual bool WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out,
            std::chrono::milliseconds timeout,
            std::chrono::steady_clock::time_point referenceTime,
            std::chrono::milliseconds& elapsed) = 0;

        static TargetInteractor& Instance();
        static void SetInstance(TargetInteractor& instance);
    };
}
