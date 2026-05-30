#include "integration_tests/hardware_in_the_loop/support/BridgeSession.hpp"
#include "integration_tests/hardware_in_the_loop/support/SerialLogger.hpp"
#include "integration_tests/hardware_in_the_loop/support/Timeouts.hpp"
#include "tools/hardware_bridge/client/common/ParseIPv4.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <chrono>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace hil
{
    namespace
    {
        std::chrono::milliseconds ToMilliseconds(std::chrono::steady_clock::duration d)
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(d);
        }

        services::IPv4Address ResolveHost(const std::string& host)
        {
            try
            {
                return tool::ParseIPv4(host);
            }
            catch (...)
            {
            }

            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;

            addrinfo* result = nullptr;
            const int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &result);
            if (rc != 0)
            {
                if (result != nullptr)
                    ::freeaddrinfo(result);
                throw std::runtime_error{ "BridgeSession: cannot resolve host '" + host + "': " + ::gai_strerror(rc) };
            }
            if (result == nullptr)
                throw std::runtime_error{ "BridgeSession: cannot resolve host '" + host + "': no address returned" };

            const auto* in = reinterpret_cast<const sockaddr_in*>(result->ai_addr);
            const uint32_t addr = ntohl(in->sin_addr.s_addr);
            ::freeaddrinfo(result);

            return services::IPv4Address{
                static_cast<uint8_t>((addr >> 24) & 0xff),
                static_cast<uint8_t>((addr >> 16) & 0xff),
                static_cast<uint8_t>((addr >> 8) & 0xff),
                static_cast<uint8_t>(addr & 0xff)
            };
        }
    }

    BridgeSession& BridgeSession::Instance()
    {
        static BridgeSession instance;
        return instance;
    }

    BridgeSession::BridgeSession()
        : config(GetBridgeConfig())
    {
        Connect();
    }

    void BridgeSession::Connect()
    {
        const auto ipv4 = ResolveHost(config.host);

        serial = std::make_unique<tool::TcpClientSerial>(
            network.ConnectionFactory(), ipv4, config.serialPort);
        can = std::make_unique<tool::TcpClientCanbus>(
            network.ConnectionFactory(), ipv4, config.canPort);

        serialTracker = std::make_unique<Tracker>(*serial);
        canTracker = std::make_unique<Tracker>(*can);

        typingTransmitter = std::make_unique<TypingTransmitter>(
            *serial,
            [this](std::chrono::milliseconds budget)
            {
                RunUntil([]
                    {
                        return false;
                    },
                    budget);
            },
            timeouts::typingInterCharDelay);

        serial->ReceiveData([this](infra::ConstByteRange data)
            {
                OnSerialBytes(data);
            });

        can->ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                CanFrame frame;
                frame.id = id;
                frame.message = data;
                frame.arrival = std::chrono::steady_clock::now();
                canQueue.push_back(std::move(frame));
            });

        const auto deadline = std::chrono::steady_clock::now() + config.connectTimeout;
        try
        {
            network.ExecuteUntil([&]
                {
                    if (serialTracker->Alive() && canTracker->Alive())
                        return true;
                    if (std::chrono::steady_clock::now() >= deadline)
                        return true;
                    return false;
                });
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error{
                std::string{ "BridgeSession: network failure during connect: " } + e.what()
            };
        }

        if (!serialTracker->Alive() || !canTracker->Alive())
        {
            throw std::runtime_error{
                "BridgeSession: failed to connect to hardware_bridge at " + config.host +
                " (serial=" + std::to_string(config.serialPort) +
                " can=" + std::to_string(config.canPort) +
                "). Is bridge_server.py running?"
            };
        }
    }

    void BridgeSession::RunUntil(const std::function<bool()>& predicate,
        std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        network.ExecuteUntil([&]
            {
                if (predicate())
                    return true;
                return std::chrono::steady_clock::now() >= deadline;
            });
    }

    void BridgeSession::ClearSerialLines()
    {
        serialLines.clear();
        lastSerialLine.clear();
        serialBuffer.clear();
        promptSeen = false;
        lastSerialDuration = std::chrono::milliseconds{ 0 };
    }

    bool BridgeSession::SendSerial(const std::string& command,
        std::chrono::milliseconds timeout)
    {
        if (!serialTracker || !serialTracker->Alive())
            throw std::runtime_error{ "BridgeSession: serial bridge disconnected" };

        {
            std::ostringstream os;
            os << "SendSerial(\"" << command << "\", timeout=" << timeout.count() << "ms)";
            SerialLogger::Instance().ApiCall(os.str());
        }

        RunUntil([]
            {
                return false;
            },
            timeouts::preDrain);
        ClearSerialLines();

        const auto sendStart = std::chrono::steady_clock::now();
        TransmitSerial(command + "\r\n");

        RunUntil([this]
            {
                return !serialLines.empty() || promptSeen;
            },
            timeout);

        DrainUntilQuiet(timeout);

        if (!serialLines.empty() && serialLines.front() == command)
            serialLines.erase(serialLines.begin());

        lastSerialLine.clear();
        for (auto it = serialLines.rbegin(); it != serialLines.rend(); ++it)
        {
            if (!it->empty() && *it != "> ")
            {
                lastSerialLine = *it;
                break;
            }
        }

        lastSerialDuration = ToMilliseconds(std::chrono::steady_clock::now() - sendStart);
        const bool ok = promptSeen || !serialLines.empty() || !lastSerialLine.empty();
        {
            std::ostringstream os;
            os << "SendSerial -> " << (ok ? "true" : "false")
               << " (duration=" << lastSerialDuration.count() << "ms"
               << ", lines=" << serialLines.size()
               << ", promptSeen=" << (promptSeen ? "true" : "false") << ")";
            SerialLogger::Instance().ApiResult(os.str());
        }
        return ok;
    }

    bool BridgeSession::WaitForPrompt(std::chrono::milliseconds timeout)
    {
        if (!serialTracker || !serialTracker->Alive())
            return false;

        {
            std::ostringstream os;
            os << "WaitForPrompt(timeout=" << timeout.count() << "ms)";
            SerialLogger::Instance().ApiCall(os.str());
        }

        ClearSerialLines();
        TransmitSerial("\r\n");

        RunUntil([this]
            {
                return promptSeen;
            },
            timeout);
        DrainUntilQuiet(timeout);

        const bool ok = promptSeen;
        ClearSerialLines();

        {
            std::ostringstream os;
            os << "WaitForPrompt -> " << (ok ? "true" : "false");
            SerialLogger::Instance().ApiResult(os.str());
        }
        return ok;
    }

    bool BridgeSession::FlushAndAccumulate(std::chrono::milliseconds timeout)
    {
        if (!serialTracker || !serialTracker->Alive())
            return false;

        {
            std::ostringstream os;
            os << "FlushAndAccumulate(timeout=" << timeout.count() << "ms)";
            SerialLogger::Instance().ApiCall(os.str());
        }

        const std::size_t linesBefore = serialLines.size();
        promptSeen = false;

        TransmitSerial("\r\n");
        RunUntil([this]
            {
                return promptSeen;
            },
            timeout);
        DrainUntilQuiet(timeout);

        const bool ok = promptSeen;
        const std::size_t newLines = serialLines.size() - linesBefore;

        {
            std::ostringstream os;
            os << "FlushAndAccumulate -> " << (ok ? "true" : "false")
               << " (newLines=" << newLines << ", total=" << serialLines.size() << ")";
            SerialLogger::Instance().ApiResult(os.str());
        }
        return ok;
    }

    void BridgeSession::OnSerialBytes(infra::ConstByteRange data)
    {
        SerialLogger::Instance().RxBytes(data);
        for (auto byte : data)
            AppendSerialByte(static_cast<char>(byte));
    }

    void BridgeSession::AppendSerialByte(char ch)
    {
        if (ch == '\n')
        {
            if (!serialBuffer.empty() && serialBuffer.back() == '\r')
                serialBuffer.pop_back();
            EmitSerialLine(std::move(serialBuffer));
            serialBuffer.clear();
            return;
        }

        serialBuffer += ch;

        if (serialBuffer.size() >= 2 &&
            serialBuffer[serialBuffer.size() - 2] == '>' &&
            serialBuffer[serialBuffer.size() - 1] == ' ')
        {
            std::string content = serialBuffer.substr(0, serialBuffer.size() - 2);
            if (!content.empty() && content.back() == '\r')
                content.pop_back();
            EmitSerialLine(std::move(content));
            promptSeen = true;
            serialBuffer.clear();
            SerialLogger::Instance().Prompt();
        }
    }

    void BridgeSession::EmitSerialLine(std::string line)
    {
        if (line.empty())
            return;
        SerialLogger::Instance().Line(line);
        lastSerialLine = line;
        serialLines.push_back(std::move(line));
    }

    void BridgeSession::DrainUntilQuiet(std::chrono::milliseconds budget)
    {
        const auto deadline = std::chrono::steady_clock::now() + budget;
        while (std::chrono::steady_clock::now() < deadline)
        {
            const auto linesBefore = serialLines.size();
            const auto bufferBefore = serialBuffer.size();
            const bool promptBefore = promptSeen;
            RunUntil([]
                {
                    return false;
                },
                timeouts::idleWindow);
            if (serialLines.size() == linesBefore &&
                serialBuffer.size() == bufferBefore &&
                promptSeen == promptBefore)
                break;
        }
    }

    void BridgeSession::TransmitSerial(const std::string& payload)
    {
        SerialLogger::Instance().TxBytes(payload);
        typingTransmitter->Send(payload);
    }

    bool BridgeSession::DrainSerial(std::chrono::milliseconds timeout)
    {
        if (!serialTracker || !serialTracker->Alive())
            throw std::runtime_error{ "BridgeSession: serial bridge disconnected" };

        {
            std::ostringstream os;
            os << "DrainSerial(timeout=" << timeout.count() << "ms)";
            SerialLogger::Instance().ApiCall(os.str());
        }

        ClearSerialLines();
        RunUntil([]
            {
                return false;
            },
            timeout);

        const bool ok = !serialLines.empty();
        {
            std::ostringstream os;
            os << "DrainSerial -> " << (ok ? "true" : "false")
               << " (lines=" << serialLines.size() << ")";
            SerialLogger::Instance().ApiResult(os.str());
        }
        return ok;
    }

    void BridgeSession::ClearCanFrames()
    {
        canQueue.clear();
    }

    bool BridgeSession::SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
        std::chrono::milliseconds timeout)
    {
        if (!canTracker || !canTracker->Alive())
            throw std::runtime_error{ "BridgeSession: CAN bridge disconnected" };

        bool completed = false;
        bool success = false;
        can->SendData(id, message, [&completed, &success](bool ok)
            {
                completed = true;
                success = ok;
            });

        RunUntil([&]
            {
                return completed;
            },
            timeout);
        return completed && success;
    }

    bool BridgeSession::WaitForCanFrame(hal::Can::Id expectedId,
        CanFrame& outFrame,
        std::chrono::milliseconds timeout,
        std::chrono::steady_clock::time_point referenceTime,
        std::chrono::milliseconds& outElapsed)
    {
        if (!canTracker || !canTracker->Alive())
            throw std::runtime_error{ "BridgeSession: CAN bridge disconnected" };

        const auto matches = [&](const CanFrame& f)
        {
            if (f.id.Is29BitId() != expectedId.Is29BitId())
                return false;
            if (expectedId.Is29BitId())
                return f.id.Get29BitId() == expectedId.Get29BitId();
            return f.id.Get11BitId() == expectedId.Get11BitId();
        };

        while (!canQueue.empty())
        {
            auto frame = canQueue.front();
            canQueue.pop_front();
            if (matches(frame))
            {
                outFrame = std::move(frame);
                const auto ref = referenceTime.time_since_epoch().count() != 0
                                     ? referenceTime
                                     : outFrame.arrival;
                outElapsed = ToMilliseconds(outFrame.arrival - ref);
                return true;
            }
        }

        bool found = false;
        RunUntil([&]
            {
                while (!canQueue.empty())
                {
                    auto frame = canQueue.front();
                    canQueue.pop_front();
                    if (matches(frame))
                    {
                        outFrame = std::move(frame);
                        found = true;
                        return true;
                    }
                }
                return false;
            },
            timeout);

        if (!found)
        {
            outElapsed = timeout;
            return false;
        }

        const auto ref = referenceTime.time_since_epoch().count() != 0
                             ? referenceTime
                             : outFrame.arrival;
        outElapsed = ToMilliseconds(outFrame.arrival - ref);
        return true;
    }
}
