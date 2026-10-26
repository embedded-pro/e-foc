#include "integration_tests/support/interactor/hardware/BridgeSession.hpp"
#include "integration_tests/support/interactor/hardware/SerialLogger.hpp"
#include "integration_tests/support/interactor/hardware/Timeouts.hpp"
#include "tools/hardware_bridge/client/common/ResolveIPv4.hpp"
#include <algorithm>
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

        struct SendResult
        {
            bool completed{ false };
            bool success{ false };
        };
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
        const auto ipv4 = tool::ResolveIPv4(config.host);
        ConnectSerial(ipv4);
        ConnectCan(ipv4);
        AwaitConnections();
    }

    bool BridgeSession::Connected() const
    {
        return serialTracker && serialTracker->Alive() && canTracker && canTracker->Alive();
    }

    void BridgeSession::Reconnect()
    {
        SerialLogger::Instance().Note("bridge", std::string{ "reconnecting (serial " } + (serialTracker && serialTracker->Alive() ? "up" : "down") +
                                                    ", can " + (canTracker && canTracker->Alive() ? "up" : "down") + ")");

        const auto ipv4 = tool::ResolveIPv4(config.host);
        if (!serialTracker || !serialTracker->Alive())
        {
            typingTransmitter.reset();
            serialTracker.reset();
            serial.reset();
            serialBuffer.clear();
            ConnectSerial(ipv4);
        }

        if (!canTracker || !canTracker->Alive())
        {
            canTracker.reset();
            can.reset();
            ConnectCan(ipv4);
        }

        AwaitConnections();
    }

    void BridgeSession::EnsureConnected()
    {
        if (!Connected())
            Reconnect();
    }

    void BridgeSession::ConnectSerial(services::IPv4Address address)
    {
        serial = std::make_unique<tool::TcpClientSerial>(
            network.ConnectionFactory(), address, config.serialPort);
        serialTracker = std::make_unique<Tracker>(*serial);

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
    }

    void BridgeSession::ConnectCan(services::IPv4Address address)
    {
        can = std::make_unique<tool::TcpClientCanbus>(
            network.ConnectionFactory(), address, config.canPort);
        canTracker = std::make_unique<Tracker>(*can);

        can->ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                CanFrame frame;
                frame.id = id;
                frame.message = data;
                frame.arrival = std::chrono::steady_clock::now();
                receivedFrames.push_back(frame);
                canQueue.push_back(std::move(frame));
            });
    }

    void BridgeSession::AwaitConnections()
    {
        const auto deadline = std::chrono::steady_clock::now() + config.connectTimeout;
        try
        {
            network.ExecuteUntil([&]
                {
                    if (serialTracker->Settled() && canTracker->Settled())
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
        discardedSerialLines += serialLines.size();
        serialLines.clear();
        lastSerialLine.clear();
        serialBuffer.clear();
        promptSeen = false;
        lastSerialDuration = std::chrono::milliseconds{ 0 };
    }

    bool BridgeSession::SendSerial(const std::string& command,
        std::chrono::milliseconds timeout)
    {
        EnsureConnected();

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
        {
            serialLines.erase(serialLines.begin());
            ++discardedSerialLines;
        }

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
        EnsureConnected();

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

    bool BridgeSession::WaitForRestart(const std::string& bannerLine, std::chrono::milliseconds timeout)
    {
        {
            std::ostringstream os;
            os << "WaitForRestart(\"" << bannerLine << "\", timeout=" << timeout.count() << "ms)";
            SerialLogger::Instance().ApiCall(os.str());
        }

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        bool reconnected = false;
        while (!reconnected && !SerialLinesContain(bannerLine) && std::chrono::steady_clock::now() < deadline)
        {
            const auto slice = std::min(timeouts::reconnectPoll, ToMilliseconds(deadline - std::chrono::steady_clock::now()));
            if (Connected())
            {
                RunUntil([this, &bannerLine]
                    {
                        return SerialLinesContain(bannerLine) || !Connected();
                    },
                    slice);
                continue;
            }

            try
            {
                Reconnect();
                reconnected = true;
            }
            catch (const std::runtime_error& error)
            {
                SerialLogger::Instance().Note("bridge", error.what());
                RunUntil([]
                    {
                        return false;
                    },
                    slice);
            }
        }

        const bool banner = SerialLinesContain(bannerLine);
        {
            std::ostringstream os;
            os << "WaitForRestart -> " << (banner || reconnected ? "true" : "false")
               << " (banner=" << (banner ? "true" : "false") << ", reconnected=" << (reconnected ? "true" : "false") << ")";
            SerialLogger::Instance().ApiResult(os.str());
        }
        return banner || reconnected;
    }

    bool BridgeSession::SerialLinesContain(const std::string& needle) const
    {
        return std::any_of(serialLines.begin(), serialLines.end(), [&needle](const std::string& line)
            {
                return line.find(needle) != std::string::npos;
            });
    }

    bool BridgeSession::FlushAndAccumulate(std::chrono::milliseconds timeout)
    {
        EnsureConnected();

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

        if (!serialBuffer.empty())
        {
            if (serialBuffer.back() == '\r')
                serialBuffer.pop_back();
            EmitSerialLine(std::move(serialBuffer));
            serialBuffer.clear();
        }
    }

    void BridgeSession::TransmitSerial(const std::string& payload)
    {
        SerialLogger::Instance().TxBytes(payload);
        typingTransmitter->Send(payload);
    }

    bool BridgeSession::DrainSerial(std::chrono::milliseconds timeout)
    {
        EnsureConnected();

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
        receivedFrames.clear();
    }

    bool BridgeSession::SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
        std::chrono::milliseconds timeout)
    {
        if (!canTracker || !canTracker->Alive())
            throw std::runtime_error{ "BridgeSession: CAN bridge disconnected" };

        // The callback can still fire after a timeout, so it shares ownership of the result
        auto result = std::make_shared<SendResult>();
        can->SendData(id, message, [result](bool ok)
            {
                result->completed = true;
                result->success = ok;
            });

        RunUntil([&result]
            {
                return result->completed;
            },
            timeout);
        return result->completed && result->success;
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
