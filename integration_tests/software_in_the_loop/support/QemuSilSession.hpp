#pragma once
#include <chrono>
#include <cstdint>
#include <string>

namespace integration
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

    private:
        pid_t pid{ -1 };
        int stdinFd{ -1 };
        int stdoutFd{ -1 };
    };
}
