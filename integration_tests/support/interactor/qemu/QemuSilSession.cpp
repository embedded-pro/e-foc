#include "integration_tests/support/interactor/qemu/QemuSilSession.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace
{
    constexpr const char* kIcountShift = "shift=3";
    constexpr const char* kPlantRecordName = "plant.rec";
    constexpr const char* kDebugLogName = "debug.log";
    constexpr std::string_view kTransmittedPrefix{ "CAN_TX " };
    constexpr auto kPumpSlice = std::chrono::milliseconds{ 10 };
    constexpr auto kGdbAttachTimeout = std::chrono::milliseconds{ 300000 };
    constexpr auto kStopTimeout = std::chrono::milliseconds{ 2000 };

    bool SilVerbose()
    {
        return std::getenv("SIL_VERBOSE") != nullptr;
    }

    int HexValue(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }

    std::string SocketPath(const char* channel)
    {
        static uint32_t sequence = 0;
        return "/tmp/qemu_sil_" + std::string{ channel } + "_" + std::to_string(getpid()) + "_" + std::to_string(sequence++) + ".sock";
    }

    std::string EncodeCanFrame(hal::Can::Id id, const hal::Can::Message& data)
    {
        const uint32_t rawId = id.Is11BitId() ? id.Get11BitId() : id.Get29BitId();

        std::array<char, 17> payload{};
        std::size_t pos = 0;
        for (const uint8_t byte : data)
        {
            payload[pos++] = "0123456789abcdef"[(byte >> 4) & 0xF];
            payload[pos++] = "0123456789abcdef"[byte & 0xF];
        }

        std::array<char, 16> idText{};
        std::snprintf(idText.data(), idText.size(), "%08lx", static_cast<unsigned long>(rawId));

        return "CAN_RX " + std::string{ idText.data() } + " " + std::string{ payload.data(), pos };
    }

    std::optional<sil::QemuSilSession::CanFrame> ParseCanFrame(const std::string& line)
    {
        if (!line.starts_with(kTransmittedPrefix))
            return std::nullopt;

        const auto idEnd = line.find(' ', kTransmittedPrefix.size());
        if (idEnd == std::string::npos || idEnd == kTransmittedPrefix.size())
            return std::nullopt;

        uint32_t rawId = 0;
        for (std::size_t i = kTransmittedPrefix.size(); i < idEnd; ++i)
        {
            const int digit = HexValue(line[i]);
            if (digit < 0)
                return std::nullopt;
            rawId = (rawId << 4) | static_cast<uint32_t>(digit);
        }

        sil::QemuSilSession::CanFrame frame{ hal::Can::Id::Create29BitId(rawId), {} };
        for (std::size_t i = idEnd + 1; i + 1 < line.size() && !frame.message.full(); i += 2)
        {
            const int hi = HexValue(line[i]);
            const int lo = HexValue(line[i + 1]);
            if (hi < 0 || lo < 0)
                break;
            frame.message.push_back(static_cast<uint8_t>((hi << 4) | lo));
        }

        return frame;
    }
}

namespace sil
{
    QemuSilSession::~QemuSilSession()
    {
        Stop();
        RemoveScenarioDirectory();
    }

    bool QemuSilSession::CreateScenarioDirectory()
    {
        RemoveScenarioDirectory();

        static uint32_t sequence = 0;
        scenarioDirectory = "/tmp/e_foc_sil_" + std::to_string(getpid()) + "_" + std::to_string(sequence++);
        return ::mkdir(scenarioDirectory.c_str(), 0700) == 0;
    }

    void QemuSilSession::RemoveScenarioDirectory()
    {
        if (scenarioDirectory.empty())
            return;

        if (debugLog.is_open())
            debugLog.close();

        if (SilVerbose())
            std::fprintf(stderr, "[QEMU] keeping scenario directory %s\n", scenarioDirectory.c_str());
        else
        {
            std::error_code error;
            std::filesystem::remove_all(scenarioDirectory, error);
        }

        scenarioDirectory.clear();
    }

    const std::string& QemuSilSession::ScenarioDirectory() const
    {
        return scenarioDirectory;
    }

    bool QemuSilSession::WriteScenarioFile(const std::string& name, const std::vector<uint8_t>& contents) const
    {
        if (scenarioDirectory.empty())
            return false;

        const std::string path = scenarioDirectory + "/" + name;
        std::FILE* file = std::fopen(path.c_str(), "wb");
        if (file == nullptr)
            return false;

        const bool written = contents.empty() ||
                             std::fwrite(contents.data(), 1, contents.size(), file) == contents.size();
        std::fclose(file);
        return written;
    }

    bool QemuSilSession::Restart(const std::string& elfPath)
    {
        Stop();
        return Start(elfPath);
    }

    bool QemuSilSession::Start(const std::string& elfPath)
    {
        signal(SIGPIPE, SIG_IGN);

        if (scenarioDirectory.empty() && !CreateScenarioDirectory())
            return false;

        char resolvedElf[PATH_MAX]{};
        if (realpath(elfPath.c_str(), resolvedElf) == nullptr)
        {
            std::fprintf(stderr, "[QEMU] cannot resolve ELF path %s\n", elfPath.c_str());
            return false;
        }
        const std::string absoluteElf{ resolvedElf };

        std::error_code error;
        std::filesystem::remove(scenarioDirectory + "/" + kPlantRecordName, error);
        plantRecordOffset = 0;
        plantRecordPartial.clear();
        debugPartial.clear();
        canPartial.clear();
        nextFrame = receivedFrames.size();
        exited = false;

        if (!debugLog.is_open())
            debugLog.open(scenarioDirectory + "/" + kDebugLogName, std::ios::out | std::ios::app);

        terminalPath = SocketPath("terminal");
        const std::string canPath = SocketPath("can");
        unlink(terminalPath.c_str());
        unlink(canPath.c_str());

        const std::string terminalChardev = "socket,id=terminal,path=" + terminalPath + ",server=on,wait=off";
        const std::string canChardev = "socket,id=can,path=" + canPath + ",server=on,wait=off";

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

            if (chdir(scenarioDirectory.c_str()) != 0)
                _exit(1);

            const bool gdbMode = (std::getenv("SIL_GDB") != nullptr);
            std::vector<const char*> argv = {
                "qemu-system-arm",
                "-M",
                "mps2-an386",
                "-nographic",
                "-chardev",
                terminalChardev.c_str(),
                "-chardev",
                canChardev.c_str(),
                "-semihosting-config",
                "enable=on",
                "-serial",
                "chardev:terminal",
                "-serial",
                "chardev:can",
                "-kernel",
                absoluteElf.c_str(),
            };
            const char* icountOverride = std::getenv("SIL_ICOUNT");
            argv.push_back("-icount");
            argv.push_back(icountOverride != nullptr ? icountOverride : kIcountShift);
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

        close(pipefd[1]);
        outPipeFd = pipefd[0];

        const bool connected = ConnectSocket(canSockFd, canPath);
        unlink(canPath.c_str());

        if (!connected)
        {
            Stop();
            return false;
        }

        if (SilVerbose())
            std::fprintf(stderr, "[QEMU] terminal socket %s\n", terminalPath.c_str());

        if (std::getenv("SIL_GDB") != nullptr)
            PumpUntil(std::chrono::steady_clock::now() + kGdbAttachTimeout, [this]()
                {
                    return debugLinesSeen != 0 || nextFrame != receivedFrames.size();
                });

        return true;
    }

    bool QemuSilSession::ConnectSocket(int& fd, const std::string& path) const
    {
        for (int attempt = 0; attempt < 50; ++attempt)
        {
            fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0)
            {
                usleep(100000);
                continue;
            }

            sockaddr_un address{};
            address.sun_family = AF_UNIX;
            std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);

            if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0)
                return true;

            close(fd);
            fd = -1;
            usleep(100000);
        }

        return false;
    }

    void QemuSilSession::Stop()
    {
        if (pid < 0)
            return;

        for (int* fd : { &outPipeFd, &canSockFd })
            if (*fd >= 0)
            {
                close(*fd);
                *fd = -1;
            }

        unlink(terminalPath.c_str());

        kill(pid, SIGTERM);

        int status = 0;
        const auto deadline = std::chrono::steady_clock::now() + kStopTimeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (waitpid(pid, &status, WNOHANG) == pid)
            {
                pid = -1;
                return;
            }
            usleep(1000);
        }

        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        pid = -1;
    }

    bool QemuSilSession::IsRunning() const
    {
        return pid >= 0;
    }

    void QemuSilSession::ResetCapture()
    {
        debugTail.clear();
        debugLinesSeen = 0;
        receivedFrames.clear();
        nextFrame = 0;
    }

    std::string QemuSilSession::DebugLogTail() const
    {
        std::string tail;
        for (const auto& line : debugTail)
            tail += line + "\n";
        return tail;
    }

    bool QemuSilSession::SendCanFrame(hal::Can::Id id, const hal::Can::Message& data)
    {
        if (canSockFd < 0)
            return false;

        const auto line = EncodeCanFrame(id, data);
        if (SilVerbose())
            std::fprintf(stderr, "[host->QEMU can] %s\n", line.c_str());

        const std::string text = line + "\n";
        return write(canSockFd, text.data(), text.size()) == static_cast<ssize_t>(text.size());
    }

    bool QemuSilSession::WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out, std::chrono::milliseconds timeout)
    {
        return PumpUntil(std::chrono::steady_clock::now() + timeout, [this, expectedId, &out]()
            {
                return TakeFrame(expectedId, out);
            });
    }

    const std::vector<QemuSilSession::CanFrame>& QemuSilSession::ReceivedFrames() const
    {
        return receivedFrames;
    }

    std::vector<std::string> QemuSilSession::TakePlantRecord(std::chrono::milliseconds timeout)
    {
        std::vector<std::string> lines;
        PumpUntil(std::chrono::steady_clock::now() + timeout, [this, &lines]()
            {
                ReadPlantRecord(lines);
                return !lines.empty();
            });
        return lines;
    }

    bool QemuSilSession::Pump(std::chrono::milliseconds timeout)
    {
        std::array<pollfd, 2> fds{ { { outPipeFd, POLLIN, 0 }, { canSockFd, POLLIN, 0 } } };
        if (poll(fds.data(), fds.size(), static_cast<int>(timeout.count())) <= 0)
            return false;

        bool progressed = false;
        if ((fds[0].revents & (POLLIN | POLLHUP)) != 0)
            progressed |= ReadLines(outPipeFd, debugPartial, [this](std::string line)
                {
                    OnDebugLine(std::move(line));
                });

        if ((fds[1].revents & (POLLIN | POLLHUP)) != 0)
            progressed |= ReadLines(canSockFd, canPartial, [this](std::string line)
                {
                    OnCanLine(line);
                });

        return progressed;
    }

    bool QemuSilSession::PumpUntil(std::chrono::steady_clock::time_point deadline, const std::function<bool()>& done)
    {
        while (true)
        {
            if (done())
                return true;

            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            if (remaining <= std::chrono::milliseconds{ 0 } || exited || outPipeFd < 0)
                return false;

            Pump(std::min(remaining, kPumpSlice));
        }
    }

    bool QemuSilSession::ReadLines(int fd, std::string& partial, const std::function<void(std::string)>& onLine)
    {
        std::array<char, 4096> buffer{};
        const ssize_t received = read(fd, buffer.data(), buffer.size());
        if (received <= 0)
        {
            if (fd == outPipeFd)
                exited = true;
            else if (fd == canSockFd)
            {
                close(canSockFd);
                canSockFd = -1;
            }
            return false;
        }

        for (ssize_t i = 0; i != received; ++i)
        {
            const char c = buffer[static_cast<std::size_t>(i)];
            if (c == '\n')
            {
                if (!partial.empty() && partial.back() == '\r')
                    partial.pop_back();
                onLine(std::move(partial));
                partial.clear();
            }
            else
                partial += c;
        }

        return true;
    }

    void QemuSilSession::OnDebugLine(std::string line)
    {
        if (SilVerbose())
            std::fprintf(stderr, "[QEMU->host] %s\n", line.c_str());

        if (debugLog.is_open())
            debugLog << line << '\n';

        debugTail.push_back(std::move(line));
        if (debugTail.size() > debugTailLines)
            debugTail.pop_front();

        ++debugLinesSeen;
    }

    void QemuSilSession::OnCanLine(const std::string& line)
    {
        if (SilVerbose())
            std::fprintf(stderr, "[QEMU can->host] %s\n", line.c_str());

        if (const auto frame = ParseCanFrame(line))
            receivedFrames.push_back(*frame);
    }

    bool QemuSilSession::TakeFrame(hal::Can::Id expectedId, hal::Can::Message& out)
    {
        for (; nextFrame < receivedFrames.size(); ++nextFrame)
            if (receivedFrames[nextFrame].id == expectedId)
            {
                out = receivedFrames[nextFrame++].message;
                return true;
            }

        return false;
    }

    void QemuSilSession::ReadPlantRecord(std::vector<std::string>& lines)
    {
        const auto path = scenarioDirectory + "/" + kPlantRecordName;
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error)
            return;

        if (size < plantRecordOffset)
        {
            plantRecordOffset = 0;
            plantRecordPartial.clear();
        }

        if (size == plantRecordOffset)
            return;

        std::ifstream file{ path, std::ios::binary };
        file.seekg(static_cast<std::streamoff>(plantRecordOffset));
        std::string appended(static_cast<std::size_t>(size - plantRecordOffset), '\0');
        file.read(appended.data(), static_cast<std::streamsize>(appended.size()));
        appended.resize(static_cast<std::size_t>(file.gcount()));
        plantRecordOffset += appended.size();

        for (const char c : appended)
        {
            if (c == '\n')
            {
                lines.push_back(std::move(plantRecordPartial));
                plantRecordPartial.clear();
            }
            else
                plantRecordPartial += c;
        }
    }
}
