#include "integration_tests/support/interactor/qemu/QemuSilSession.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace
{
    bool SilVerbose()
    {
        return std::getenv("SIL_VERBOSE") != nullptr;
    }
}

namespace sil
{
    QemuSilSession::~QemuSilSession()
    {
        Stop();
    }

    bool QemuSilSession::Start(const std::string& elfPath)
    {
        signal(SIGPIPE, SIG_IGN);

        // Erase the NVM image so every scenario starts with blank calibration.
        std::remove("/tmp/eeprom.bin");

        const std::string pidStr = std::to_string(getpid());
        const std::string inPath = "/tmp/qemu_sil_in_" + pidStr + ".sock";
        unlink(inPath.c_str());

        // in: UNIX socket for serial0 (CMSDK UART) — CAN_RX frames from test.
        // wait=off so QEMU starts immediately; the test connects after READY.
        const std::string inChardevArg = "socket,id=in,path=" + inPath + ",server=on,wait=off";

        // Pipe for QEMU stdout: firmware printf (semihosting target=native) goes
        // here, giving us CAN_TX lines and the READY sentinel.
        int pipefd[2];
        if (pipe(pipefd) != 0)
            return false;

        pid = fork();
        if (pid < 0)
        {
            close(pipefd[0]);
            close(pipefd[1]);
            return false;
        }

        if (pid == 0)
        {
            // Child: wire QEMU's stdout/stderr → pipe write end, stdin → /dev/null.
            close(pipefd[0]);
            const int devNull = open("/dev/null", O_RDWR);
            if (devNull >= 0)
            {
                dup2(devNull, STDIN_FILENO);
                close(devNull);
            }
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);

            const bool gdbMode = (std::getenv("SIL_GDB") != nullptr);
            std::vector<const char*> argv = {
                "qemu-system-arm",
                "-M",
                "mps2-an386",
                "-nographic",
                "-chardev",
                inChardevArg.c_str(),
                "-semihosting-config",
                "enable=on",
                "-serial",
                "chardev:in",
                "-kernel",
                elfPath.c_str(),
            };
            if (gdbMode)
            {
                argv.push_back("-S");
                argv.push_back("-gdb");
                argv.push_back("tcp::1234");
            }
            argv.push_back(nullptr);
            execvp("qemu-system-arm", const_cast<char* const*>(argv.data()));
            _exit(1);
        }

        // Parent: close the write end of the pipe and save the read end.
        close(pipefd[1]);
        outPipeFd = pipefd[0];
        readBufPos = 0;
        readBufLen = 0;

        std::string ready;
        const auto readyTimeout = (std::getenv("SIL_GDB") != nullptr)
            ? std::chrono::milliseconds{ 300000 }
            : std::chrono::milliseconds{ 10000 };
        if (!WaitFor("READY", ready, readyTimeout))
        {
            Stop();
            return false;
        }

        // Connect to the UART input socket (created by QEMU with wait=off).
        for (int attempt = 0; attempt < 50; ++attempt)
        {
            inSockFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
            if (inSockFd < 0)
            {
                usleep(100000);
                continue;
            }

            struct sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            std::strncpy(addr.sun_path, inPath.c_str(), sizeof(addr.sun_path) - 1);

            if (connect(inSockFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0)
                break;

            close(inSockFd);
            inSockFd = -1;
            usleep(100000);
        }
        unlink(inPath.c_str());

        if (inSockFd < 0)
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

        if (outPipeFd >= 0)
        {
            close(outPipeFd);
            outPipeFd = -1;
        }

        if (inSockFd >= 0)
        {
            close(inSockFd);
            inSockFd = -1;
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
        if (inSockFd < 0)
            return false;

        const std::string data = line + "\n";
        if (SilVerbose())
            std::fprintf(stderr, "[host->QEMU] %s\n", line.c_str());
        const ssize_t written = write(inSockFd, data.data(), data.size());
        return written == static_cast<ssize_t>(data.size());
    }

    bool QemuSilSession::FillReadBuffer(int timeoutMs)
    {
        pollfd pfd{ outPipeFd, POLLIN, 0 };
        const int ready = poll(&pfd, 1, timeoutMs);
        if (ready <= 0)
            return false;
        if ((pfd.revents & POLLHUP) != 0 && (pfd.revents & POLLIN) == 0)
            return false;

        const ssize_t n = read(outPipeFd, readBuf.data(), readBuf.size());
        if (n <= 0)
            return false;

        readBufPos = 0;
        readBufLen = static_cast<int>(n);
        return true;
    }

    bool QemuSilSession::ReadLine(std::string& line, std::chrono::milliseconds timeout)
    {
        if (outPipeFd < 0)
            return false;

        line.clear();
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (true)
        {
            // Drain buffered bytes first before calling poll/read.
            while (readBufPos < readBufLen)
            {
                const char ch = readBuf[readBufPos++];
                if (ch == '\n')
                {
                    if (SilVerbose())
                        std::fprintf(stderr, "[QEMU->host] %s\n", line.c_str());
                    return true;
                }
                line += ch;
            }

            const auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::milliseconds{ 0 })
                return false;

            const int ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count());
            if (!FillReadBuffer(ms))
                return false;
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

            if (candidate.find("ABORT") == 0)
            {
                std::fprintf(stderr, "[QEMU] firmware crash: %s\n", candidate.c_str());
                return false;
            }

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

            if (line.find("ABORT") == 0)
            {
                std::fprintf(stderr, "[QEMU] firmware crash: %s\n", line.c_str());
                return false;
            }

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
        {
            if (SilVerbose())
                std::fprintf(stderr, "[CAN drop] parsed=%08lx expected=%08lx line=%s\n",
                    static_cast<unsigned long>(rawId),
                    static_cast<unsigned long>(expectedId.Is11BitId() ? expectedId.Get11BitId() : expectedId.Get29BitId()),
                    line.c_str());
            return false;
        }

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
