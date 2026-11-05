#pragma once
#include "hal/interfaces/Can.hpp"
#include <chrono>
#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

namespace sil
{
    class QemuSilSession
    {
    public:
        struct CanFrame
        {
            hal::Can::Id id;
            hal::Can::Message message;
        };

        struct Images
        {
            std::string bootloaderElf;
            std::optional<std::string> slotImage;
        };

        QemuSilSession() = default;
        ~QemuSilSession();

        bool Start(const Images& images);
        bool Restart(const Images& images);
        void Stop();

        bool CreateScenarioDirectory();
        void RemoveScenarioDirectory();
        const std::string& ScenarioDirectory() const;
        bool WriteScenarioFile(const std::string& name, const std::vector<uint8_t>& contents) const;

        bool IsRunning() const;
        void ResetCapture();

        std::string DebugLogTail() const;

        bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& data, std::chrono::milliseconds timeout);
        bool WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out, std::chrono::milliseconds timeout);
        const std::vector<CanFrame>& ReceivedFrames() const;

        std::vector<std::string> TakePlantRecord(std::chrono::milliseconds timeout);

    private:
        struct Launch
        {
            std::string elf;
            std::optional<std::string> slotLoader;
        };

        static std::optional<Launch> ResolveLaunch(const Images& images);
        void ResetRun();
        std::string PrepareSockets();
        bool Spawn(const Launch& launch, const std::string& canPath);
        [[noreturn]] void ExecEmulator(const Launch& launch, const std::string& canPath, int outputFd) const;
        bool Attach(const std::string& canPath);
        bool ConnectSocket(int& fd, const std::string& path) const;
        bool Pump(std::chrono::milliseconds timeout);
        bool PumpUntil(std::chrono::steady_clock::time_point deadline, const std::function<bool()>& done);
        bool ReadLines(int fd, std::string& partial, const std::function<void(std::string)>& onLine);
        void OnDebugLine(std::string line);
        void OnCanLine(const std::string& line);
        bool TakeFrame(hal::Can::Id expectedId, hal::Can::Message& out);
        void ReadPlantRecord(std::vector<std::string>& lines);

        static constexpr std::size_t debugTailLines = 200;

        std::string scenarioDirectory;
        std::string terminalPath;
        pid_t pid{ -1 };
        bool exited{ false };
        int outPipeFd{ -1 };
        int canSockFd{ -1 };

        std::string debugPartial;
        std::size_t debugLinesSeen{ 0 };
        std::deque<std::string> debugTail;
        std::ofstream debugLog;

        std::string canPartial;
        std::vector<CanFrame> receivedFrames;
        std::size_t nextFrame{ 0 };

        std::string plantRecordPartial;
        std::uintmax_t plantRecordOffset{ 0 };
    };
}
