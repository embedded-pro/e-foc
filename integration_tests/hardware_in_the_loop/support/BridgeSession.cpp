#include "integration_tests/hardware_in_the_loop/support/BridgeSession.hpp"
#include "tools/hardware_bridge/client/common/ParseIPv4.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <netdb.h>
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
            if (rc != 0 || result == nullptr)
                throw std::runtime_error{ "BridgeSession: cannot resolve host '" + host + "': " + ::gai_strerror(rc) };

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

        serial->ReceiveData([this](infra::ConstByteRange data)
            {
                for (auto byte : data)
                {
                    const char ch = static_cast<char>(byte);
                    if (ch == '\n')
                    {
                        if (!serialBuffer.empty() && serialBuffer.back() == '\r')
                            serialBuffer.pop_back();

                        if (!serialBuffer.empty())
                        {
                            serialLines.push_back(serialBuffer);
                            lastSerialLine = serialBuffer;
                        }
                        serialBuffer.clear();
                    }
                    else
                    {
                        serialBuffer += ch;
                    }
                }
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
        lastSerialDuration = std::chrono::milliseconds{ 0 };
    }

    bool BridgeSession::SendSerial(const std::string& command,
        std::chrono::milliseconds timeout)
    {
        if (!serialTracker || !serialTracker->Alive())
            throw std::runtime_error{ "BridgeSession: serial bridge disconnected" };

        constexpr auto preDrain = std::chrono::milliseconds{ 30 };
        RunUntil([]
            {
                return false;
            },
            preDrain);
        ClearSerialLines();

        const std::string payload = command + "\r\n";
        const auto sendStart = std::chrono::steady_clock::now();
        serial->SendData(
            infra::ConstByteRange(reinterpret_cast<const uint8_t*>(payload.data()),
                reinterpret_cast<const uint8_t*>(payload.data() + payload.size())),
            infra::Function<void()>{});

        RunUntil([this]
            {
                return !serialLines.empty();
            },
            timeout);
        if (serialLines.empty())
            return false;

        constexpr auto idleWindow = std::chrono::milliseconds{ 75 };
        const auto drainDeadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < drainDeadline)
        {
            const auto linesBefore = serialLines.size();
            const auto bufferBefore = serialBuffer.size();
            RunUntil([]
                {
                    return false;
                },
                idleWindow);
            if (serialLines.size() == linesBefore && serialBuffer.size() == bufferBefore)
                break;
        }

        if (!serialLines.empty() && serialLines.front() == command)
        {
            serialLines.erase(serialLines.begin());
            lastSerialLine = serialLines.empty() ? std::string{} : serialLines.back();
        }

        lastSerialDuration = ToMilliseconds(std::chrono::steady_clock::now() - sendStart);
        return !serialLines.empty() || !lastSerialLine.empty();
    }

    bool BridgeSession::DrainSerial(std::chrono::milliseconds timeout)
    {
        if (!serialTracker || !serialTracker->Alive())
            throw std::runtime_error{ "BridgeSession: serial bridge disconnected" };

        ClearSerialLines();
        RunUntil([]
            {
                return false;
            },
            timeout);
        return !serialLines.empty();
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
