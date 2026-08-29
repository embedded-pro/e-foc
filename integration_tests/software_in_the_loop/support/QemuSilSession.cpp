#include "integration_tests/software_in_the_loop/support/QemuSilSession.hpp"
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace sil
{
    QemuSilSession::~QemuSilSession()
    {
        Stop();
    }

    bool QemuSilSession::Start(const std::string& elfPath)
    {
        int toChild[2];
        int toParent[2];

        if (pipe(toChild) != 0 || pipe(toParent) != 0)
            return false;

        pid = fork();
        if (pid < 0)
        {
            close(toChild[0]);
            close(toChild[1]);
            close(toParent[0]);
            close(toParent[1]);
            return false;
        }

        if (pid == 0)
        {
            dup2(toChild[0], STDIN_FILENO);
            dup2(toParent[1], STDOUT_FILENO);
            close(toChild[0]);
            close(toChild[1]);
            close(toParent[0]);
            close(toParent[1]);

            const char* const argv[] = {
                "qemu-system-arm",
                "-M", "mps2-an386",
                "-nographic",
                "-semihosting-config", "enable=on,target=native",
                "-kernel", elfPath.c_str(),
                nullptr
            };

            execvp("qemu-system-arm", const_cast<char* const*>(argv));
            _exit(1);
        }

        close(toChild[0]);
        close(toParent[1]);

        stdinFd = toChild[1];
        stdoutFd = toParent[0];

        std::string ready;
        if (!WaitFor("READY", ready, std::chrono::milliseconds{ 10000 }))
        {
            Stop();
            return false;
        }

        return true;
    }

    void QemuSilSession::Stop()
    {
        if (pid < 0)
            return;

        if (stdinFd >= 0)
        {
            const std::string quit = "quit\n";
            const ssize_t ignored = write(stdinFd, quit.data(), quit.size());
            (void)ignored;
            close(stdinFd);
            stdinFd = -1;
        }

        if (stdoutFd >= 0)
        {
            close(stdoutFd);
            stdoutFd = -1;
        }

        int status = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{ 2000 };
        while (std::chrono::steady_clock::now() < deadline)
        {
            const pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == pid)
            {
                pid = -1;
                return;
            }
            usleep(10000);
        }

        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        pid = -1;
    }

    bool QemuSilSession::SendLine(const std::string& line)
    {
        if (stdinFd < 0)
            return false;

        const std::string data = line + "\n";
        const ssize_t written = write(stdinFd, data.data(), data.size());
        return written == static_cast<ssize_t>(data.size());
    }

    bool QemuSilSession::ReadLine(std::string& line, std::chrono::milliseconds timeout)
    {
        if (stdoutFd < 0)
            return false;

        line.clear();
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (true)
        {
            const auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::milliseconds{ 0 })
                return false;

            const int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count());

            pollfd pfd{ stdoutFd, POLLIN, 0 };
            const int ready = poll(&pfd, 1, ms);
            if (ready <= 0)
                return false;

            if ((pfd.revents & POLLHUP) != 0 && (pfd.revents & POLLIN) == 0)
                return false;

            char ch = '\0';
            const ssize_t n = read(stdoutFd, &ch, 1);
            if (n <= 0)
                return false;

            if (ch == '\n')
                return true;

            line += ch;
        }
    }

    bool QemuSilSession::WaitFor(const std::string& prefix, std::string& line, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (true)
        {
            const auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::milliseconds{ 0 })
                return false;

            std::string candidate;
            if (!ReadLine(candidate, std::chrono::duration_cast<std::chrono::milliseconds>(remaining)))
                return false;

            if (candidate.find(prefix) == 0)
            {
                line = candidate;
                return true;
            }
        }
    }

    bool QemuSilSession::IsRunning() const
    {
        return pid >= 0;
    }

    bool QemuSilSession::SendCanFrame(hal::Can::Id id, const hal::Can::Message& data)
    {
        return SendLine(EncodeCanFrame(id, data, "CAN_RX"));
    }

    bool QemuSilSession::WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out,
        std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (true)
        {
            const auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::milliseconds{ 0 })
                return false;

            std::string line;
            if (!ReadLine(line, std::chrono::duration_cast<std::chrono::milliseconds>(remaining)))
                return false;

            if (ParseCanFrame(line, "CAN_TX", expectedId, out))
                return true;
        }
    }

    std::string QemuSilSession::EncodeCanFrame(hal::Can::Id id, const hal::Can::Message& data,
        const std::string& prefix)
    {
        const uint32_t rawId = id.Is11BitId() ? id.Get11BitId() : id.Get29BitId();

        char buf[32]{};
        int pos = 0;
        for (const uint8_t byte : data)
        {
            buf[pos++] = "0123456789abcdef"[(byte >> 4) & 0xF];
            buf[pos++] = "0123456789abcdef"[byte & 0xF];
        }
        buf[pos] = '\0';

        char idBuf[16]{};
        std::snprintf(idBuf, sizeof(idBuf), "%08lx", static_cast<unsigned long>(rawId));

        return prefix + " " + idBuf + " " + buf;
    }

    bool QemuSilSession::ParseCanFrame(const std::string& line, const std::string& prefix,
        hal::Can::Id expectedId, hal::Can::Message& out)
    {
        const std::string fullPrefix = prefix + " ";
        if (line.rfind(fullPrefix, 0) != 0)
            return false;

        const std::string rest = line.substr(fullPrefix.size());
        const auto spacePos = rest.find(' ');
        if (spacePos == std::string::npos)
            return false;

        const std::string idStr = rest.substr(0, spacePos);
        uint32_t rawId = 0;
        try
        {
            rawId = static_cast<uint32_t>(std::stoul(idStr, nullptr, 16));
        }
        catch (...)
        {
            return false;
        }

        const hal::Can::Id parsedId = hal::Can::Id::Create29BitId(rawId);
        if (parsedId != expectedId)
            return false;

        const std::string dataStr = rest.substr(spacePos + 1);
        out.clear();

        auto hexVal = [](char c) -> int
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        };

        for (std::size_t i = 0; i + 1 < dataStr.size(); i += 2)
        {
            const int hi = hexVal(dataStr[i]);
            const int lo = hexVal(dataStr[i + 1]);
            if (hi < 0 || lo < 0)
                break;
            if (out.full())
                break;
            out.push_back(static_cast<uint8_t>((hi << 4) | lo));
        }

        return true;
    }
}
