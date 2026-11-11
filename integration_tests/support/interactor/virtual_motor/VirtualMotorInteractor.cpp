#include "integration_tests/support/interactor/virtual_motor/VirtualMotorInteractor.hpp"
#include "can-lite/drivers/implementation/LoopbackSocket.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <gtest/gtest.h>
#include <poll.h>
#include <signal.h>
#include <span>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

namespace virtual_motor
{
    namespace
    {
        constexpr const char* busName{ "e-foc" };
        constexpr const char* plantFile{ "plant.bin" };
        constexpr const char* memoryFile{ "eeprom.bin" };
        constexpr const char* debugLogFile{ "debug.log" };
        constexpr uint32_t applicationBitRate{ 1000000 };
        constexpr auto pumpSlice{ std::chrono::milliseconds{ 10 } };
        constexpr auto joinTimeout{ std::chrono::seconds{ 10 } };
        constexpr auto stopTimeout{ std::chrono::seconds{ 2 } };

        bool Verbose()
        {
            return std::getenv("SIL_VERBOSE") != nullptr;
        }

        std::optional<uint16_t> FreeLoopbackPort()
        {
            const auto listener = services::loopback::Listen(0);
            if (listener == services::loopback::invalidSocket)
                return std::nullopt;
            const auto port = services::loopback::LocalPort(listener);
            services::loopback::Close(listener);
            return port;
        }

        hal::Can::Id IdOf(const services::SlcanFrame& frame)
        {
            return frame.extended ? hal::Can::Id::Create29BitId(frame.id) : hal::Can::Id::Create11BitId(frame.id);
        }
    }

    VirtualMotorInteractor::VirtualMotorInteractor(std::string program)
        : program(std::move(program))
    {}

    VirtualMotorInteractor::~VirtualMotorInteractor()
    {
        Stop();
        RemoveDirectory();
    }

    void VirtualMotorInteractor::Setup()
    {
        signal(SIGPIPE, SIG_IGN);
        ASSERT_EQ(access(program.c_str(), X_OK), 0) << "[virtual] the virtual motor program " << program << " is missing or not executable";
    }

    void VirtualMotorInteractor::Teardown()
    {}

    void VirtualMotorInteractor::BeforeScenario()
    {
        received.clear();
        nextFrame = 0;
        debugTail.clear();
        plantDescribed = false;
        if (!CreateDirectory())
            GTEST_FAIL() << "[virtual] could not create the scenario working directory";
        if (!Start())
            GTEST_FAIL() << "[virtual] the virtual motor did not start; last output:\n"
                         << DebugLog();
    }

    void VirtualMotorInteractor::AfterScenario()
    {
        Stop();
        RemoveDirectory();
    }

    bool VirtualMotorInteractor::SupportsSimulatedPlant() const
    {
        return true;
    }

    void VirtualMotorInteractor::ConfigurePlant(const std::vector<uint8_t>& record)
    {
        plantDescribed = WriteFile(plantFile, record);
        ASSERT_TRUE(plantDescribed) << "[virtual] could not write the plant description";
    }

    void VirtualMotorInteractor::ConfigureNonVolatileMemory(const std::vector<uint8_t>& image)
    {
        Stop();
        ASSERT_TRUE(WriteFile(memoryFile, image)) << "[virtual] could not write the memory image";
    }

    void VirtualMotorInteractor::RestartTarget()
    {
        Stop();
        ASSERT_TRUE(Start()) << "[virtual] the virtual motor did not restart; last output:\n"
                             << DebugLog();
    }

    void VirtualMotorInteractor::ClearCanFrames()
    {}

    bool VirtualMotorInteractor::SendCanFrame(hal::Can::Id id, const hal::Can::Message& message, std::chrono::milliseconds)
    {
        if (!bus)
            return false;

        const auto extended = id.Is29BitId();
        services::CanFrame data;
        data.assign(message.begin(), message.end());
        const auto line = services::EncodeSlcanFrame(extended ? id.Get29BitId() : id.Get11BitId(), extended, data);
        if (Verbose())
            std::fprintf(stderr, "[host->virtual can] %.*s\n", static_cast<int>(line.size()), line.data());
        return bus->Write(line);
    }

    bool VirtualMotorInteractor::WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out,
        std::chrono::milliseconds timeout, std::chrono::steady_clock::time_point, std::chrono::milliseconds& elapsed)
    {
        const auto start = std::chrono::steady_clock::now();
        const auto found = PumpUntil(start + timeout, [this, expectedId, &out]()
            {
                return TakeFrame(expectedId, out);
            });
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        return found;
    }

    std::size_t VirtualMotorInteractor::ReceivedCanFrameCount() const
    {
        return received.size();
    }

    std::optional<hal::Can::Message> VirtualMotorInteractor::FindReceivedCanFrame(hal::Can::Id id, std::size_t fromFrame) const
    {
        std::optional<hal::Can::Message> found;
        for (auto frame = received.begin() + static_cast<std::ptrdiff_t>(std::min(fromFrame, received.size())); frame != received.end(); ++frame)
            if (frame->id == id)
                found = frame->message;
        return found;
    }

    std::vector<hal::Can::Message> VirtualMotorInteractor::FindReceivedCanFrames(hal::Can::Id id, std::size_t fromFrame) const
    {
        std::vector<hal::Can::Message> found;
        for (auto frame = received.begin() + static_cast<std::ptrdiff_t>(std::min(fromFrame, received.size())); frame != received.end(); ++frame)
            if (frame->id == id)
                found.push_back(frame->message);
        return found;
    }

    std::optional<hal::Can::Id> VirtualMotorInteractor::FindReceivedCanFrameId(const std::function<bool(hal::Can::Id)>& matches, std::size_t fromFrame) const
    {
        for (auto frame = received.begin() + static_cast<std::ptrdiff_t>(std::min(fromFrame, received.size())); frame != received.end(); ++frame)
            if (matches(frame->id))
                return frame->id;
        return std::nullopt;
    }

    std::string VirtualMotorInteractor::DebugLog() const
    {
        std::string tail;
        for (const auto& line : debugTail)
            tail += line + "\n";
        return tail;
    }

    bool VirtualMotorInteractor::CreateDirectory()
    {
        RemoveDirectory();
        static uint32_t sequence{ 0 };
        directory = "/tmp/e_foc_virtual_" + std::to_string(getpid()) + "_" + std::to_string(sequence++);
        if (mkdir(directory.c_str(), 0700) != 0)
            return false;
        debugLog.open(directory + "/" + debugLogFile, std::ios::out | std::ios::app);
        return true;
    }

    void VirtualMotorInteractor::RemoveDirectory()
    {
        if (directory.empty())
            return;

        debugLog.close();
        if (Verbose())
            std::fprintf(stderr, "[virtual] keeping scenario directory %s\n", directory.c_str());
        else
        {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
        }
        directory.clear();
    }

    bool VirtualMotorInteractor::WriteFile(const std::string& name, const std::vector<uint8_t>& contents) const
    {
        std::ofstream file{ directory + "/" + name, std::ios::binary | std::ios::trunc };
        file.write(reinterpret_cast<const char*>(contents.data()), static_cast<std::streamsize>(contents.size()));
        return file.good();
    }

    bool VirtualMotorInteractor::Start()
    {
        const auto hubPort = FreeLoopbackPort();
        return hubPort.has_value() && Spawn(*hubPort) && JoinBus(*hubPort);
    }

    void VirtualMotorInteractor::Stop()
    {
        bus.reset();
        decoder.Reset();
        if (pid < 0)
            return;

        kill(pid, SIGTERM);
        const auto deadline = std::chrono::steady_clock::now() + stopTimeout;
        int status{ 0 };
        while (waitpid(pid, &status, WNOHANG) != pid)
        {
            if (std::chrono::steady_clock::now() >= deadline)
            {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                break;
            }
            usleep(1000);
        }
        pid = -1;
        ReadOutput();
        close(outputFd);
        outputFd = -1;
        outputPartial.clear();
    }

    bool VirtualMotorInteractor::Spawn(uint16_t hubPort)
    {
        std::array<int, 2> pipeFds{};
        if (pipe(pipeFds.data()) != 0)
            return false;

        pid = fork();
        if (pid == 0)
        {
            close(pipeFds[0]);
            Exec(hubPort, pipeFds[1]);
        }

        close(pipeFds[1]);
        if (pid < 0)
        {
            close(pipeFds[0]);
            return false;
        }
        outputFd = pipeFds[0];
        return true;
    }

    void VirtualMotorInteractor::Exec(uint16_t hubPort, int output) const
    {
        const int devNull = open("/dev/null", O_RDONLY);
        dup2(devNull, STDIN_FILENO);
        dup2(output, STDOUT_FILENO);
        dup2(output, STDERR_FILENO);
        if (chdir(directory.c_str()) != 0)
            _exit(EXIT_FAILURE);

        const auto port = std::to_string(hubPort);
        const std::array<const char*, 13> argv{ program.c_str(), "--bus", busName, "--plant", plantDescribed ? plantFile : "nominal",
            "--memory", memoryFile, "--host-bus", "--hub-port", port.c_str(), "--node", "0", nullptr };
        execv(program.c_str(), const_cast<char* const*>(argv.data()));
        _exit(EXIT_FAILURE);
    }

    bool VirtualMotorInteractor::JoinBus(uint16_t hubPort)
    {
        return PumpUntil(std::chrono::steady_clock::now() + joinTimeout, [this, hubPort]()
            {
                auto& port = bus.emplace(hubPort);
                if (port.Open(busName) != services::SerialPortOpenResult::opened)
                {
                    bus.reset();
                    return false;
                }
                return port.Write(services::SlcanOpenCommands(*services::SlcanBitrateCode(applicationBitRate)));
            });
    }

    bool VirtualMotorInteractor::PumpUntil(std::chrono::steady_clock::time_point deadline, const std::function<bool()>& done)
    {
        while (!done())
        {
            if (std::chrono::steady_clock::now() >= deadline || outputFd < 0)
                return false;
            Pump(pumpSlice);
        }
        return true;
    }

    void VirtualMotorInteractor::Pump(std::chrono::milliseconds timeout)
    {
        std::array<pollfd, 2> fds{ { { outputFd, POLLIN, 0 }, { bus ? static_cast<int>(bus->Handle()) : -1, POLLIN, 0 } } };
        if (poll(fds.data(), fds.size(), static_cast<int>(timeout.count())) <= 0)
            return;

        if ((fds[0].revents & (POLLIN | POLLHUP)) != 0)
            ReadOutput();
        if ((fds[1].revents & (POLLIN | POLLHUP)) != 0)
            ReadBus();
    }

    void VirtualMotorInteractor::ReadOutput()
    {
        std::array<char, 4096> buffer{};
        const auto count = read(outputFd, buffer.data(), buffer.size());
        if (count <= 0)
            return;

        for (const auto character : std::span{ buffer.data(), static_cast<std::size_t>(count) })
            if (character == '\n')
                OnOutputLine(std::exchange(outputPartial, {}));
            else if (character != '\r')
                outputPartial += character;
    }

    void VirtualMotorInteractor::ReadBus()
    {
        std::array<uint8_t, 1024> buffer{};
        const auto count = bus->Read(infra::MakeByteRange(buffer));
        if (!count.has_value())
        {
            bus.reset();
            return;
        }

        for (const auto byte : std::span{ buffer.data(), *count })
            if (const auto item = decoder.Feed(static_cast<char>(byte)); item && item->event == services::SlcanDecoder::Event::frame)
            {
                if (Verbose())
                    std::fprintf(stderr, "[virtual can->host] %08x\n", static_cast<unsigned>(item->frame.id));
                received.push_back({ IdOf(item->frame), hal::Can::Message{ item->frame.data.begin(), item->frame.data.end() } });
            }
    }

    void VirtualMotorInteractor::OnOutputLine(std::string line)
    {
        if (Verbose())
            std::fprintf(stderr, "[virtual->host] %s\n", line.c_str());
        if (debugLog.is_open())
            debugLog << line << '\n';
        debugTail.push_back(std::move(line));
        if (debugTail.size() > debugTailLines)
            debugTail.pop_front();
    }

    bool VirtualMotorInteractor::TakeFrame(hal::Can::Id expectedId, hal::Can::Message& out)
    {
        for (; nextFrame < received.size(); ++nextFrame)
            if (received[nextFrame].id == expectedId)
            {
                out = received[nextFrame++].message;
                return true;
            }
        return false;
    }
}
