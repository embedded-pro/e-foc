#include "integration_tests/support/interactor/qemu/QemuSilSession.hpp"
#include <chrono>
#include <cstdio>
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

namespace sil
{
    QemuSilSession::~QemuSilSession()
    {
        Stop();
    }

    bool QemuSilSession::Start(const std::string& elfPath)
    {
        signal(SIGPIPE, SIG_IGN);

        const std::string pidStr  = std::to_string(getpid());
        const std::string outPath = "/tmp/qemu_sil_out_" + pidStr + ".sock";
        const std::string inPath  = "/tmp/qemu_sil_in_"  + pidStr + ".sock";
        unlink(outPath.c_str());
        unlink(inPath.c_str());

        // out: semihosting output (CAN_TX, READY sentinel) — wait=on so QEMU blocks
        // until the test connects and can observe firmware output from the start.
        // in: serial/UART input (CAN_RX frames from test) — wait=off so QEMU starts
        // immediately; the test connects after READY is received.
        const std::string outChardevArg = "socket,id=out,path=" + outPath + ",server=on,wait=on";
        const std::string inChardevArg  = "socket,id=in,path="  + inPath  + ",server=on,wait=off";
        const std::string semihostingArg = "enable=on,chardev=out";

        pid = fork();
        if (pid < 0)
            return false;

        if (pid == 0)
        {
            const int devNull = open("/dev/null", O_RDWR);
            if (devNull >= 0)
            {
                dup2(devNull, STDIN_FILENO);
                dup2(devNull, STDOUT_FILENO);
                dup2(devNull, STDERR_FILENO);
                close(devNull);
            }

            const char* const argv[] = {
                "qemu-system-arm",
                "-M", "mps2-an386",
                "-nographic",
                "-chardev", outChardevArg.c_str(),
                "-chardev", inChardevArg.c_str(),
                "-semihosting-config", semihostingArg.c_str(),
                "-serial", "chardev:in",
                "-kernel", elfPath.c_str(),
                nullptr
            };
            execvp("qemu-system-arm", const_cast<char* const*>(argv));
            _exit(1);
        }

        // Parent: connect to the output socket first (blocks until QEMU creates it)
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            usleep(100000);

            outSockFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
            if (outSockFd < 0)
                continue;

            struct sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            std::strncpy(addr.sun_path, outPath.c_str(), sizeof(addr.sun_path) - 1);

            if (connect(outSockFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0)
                break;

            close(outSockFd);
            outSockFd = -1;
        }
        unlink(outPath.c_str());

        if (outSockFd < 0)
        {
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            pid = -1;
            return false;
        }

        std::string ready;
        if (!WaitFor("READY", ready, std::chrono::milliseconds{ 10000 }))
        {
            Stop();
            return false;
        }

        // Connect to the input socket (QEMU has started; "in" chardev was created with
        // wait=off so the socket file exists but no client was required at startup).
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

        if (outSockFd >= 0)
        {
            close(outSockFd);
            outSockFd = -1;
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
        const ssize_t written = write(inSockFd, data.data(), data.size());
        return written == static_cast<ssize_t>(data.size());
    }

    bool QemuSilSession::ReadLine(std::string& line, std::chrono::milliseconds timeout)
    {
        if (outSockFd < 0)
            return false;

        line.clear();
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (true)
        {
            const auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::milliseconds{ 0 })
                return false;

            const int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count());

            pollfd pfd{ outSockFd, POLLIN, 0 };
            const int ready = poll(&pfd, 1, ms);
            if (ready <= 0)
                return false;

            if ((pfd.revents & POLLHUP) != 0 && (pfd.revents & POLLIN) == 0)
                return false;

            char ch = '\0';
            const ssize_t n = read(outSockFd, &ch, 1);
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

            if (candidate.find("ABORT") == 0)
            {
                fprintf(stderr, "[QEMU] firmware crash: %s\n", candidate.c_str());
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
