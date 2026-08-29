#pragma once
#include "hal/interfaces/Can.hpp"
#include <chrono>
#include <cstdint>
#include <string>
#include <sys/types.h>

namespace sil
{
    class QemuSilSession
    {
    public:
        QemuSilSession() = default;
        ~QemuSilSession();

        bool Start(const std::string& elfPath);
        void Stop();

        bool SendLine(const std::string& line);
        bool ReadLine(std::string& line, std::chrono::milliseconds timeout);
        bool WaitFor(const std::string& prefix, std::string& line, std::chrono::milliseconds timeout);

        bool IsRunning() const;

        bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& data);
        bool WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out,
            std::chrono::milliseconds timeout);

    private:
        static std::string EncodeCanFrame(hal::Can::Id id, const hal::Can::Message& data,
            const std::string& prefix);
        static bool ParseCanFrame(const std::string& line, const std::string& prefix,
            hal::Can::Id expectedId, hal::Can::Message& out);

        pid_t pid{ -1 };
        int stdinFd{ -1 };
        int stdoutFd{ -1 };
    };
}
