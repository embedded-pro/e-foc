#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/ByteRange.hpp"
#include "integration_tests/hardware_in_the_loop/support/BridgeConfig.hpp"
#include "services/network_instantiations/NetworkAdapter.hpp"
#include "tools/hardware_bridge/client/can/TcpClientCanbus.hpp"
#include "tools/hardware_bridge/client/common/TcpClientBase.hpp"
#include "tools/hardware_bridge/client/serial/TcpClientSerial.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace hil
{
    class BridgeSession
    {
    public:
        struct CanFrame
        {
            hal::Can::Id id{ hal::Can::Id::Create11BitId(0) };
            hal::Can::Message message;
            std::chrono::steady_clock::time_point arrival{};
        };

        static BridgeSession& Instance();

        const BridgeConfig& Config() const
        {
            return config;
        }

        void ClearSerialLines();
        bool SendSerial(const std::string& command, std::chrono::milliseconds timeout);
        bool DrainSerial(std::chrono::milliseconds timeout);

        const std::vector<std::string>& SerialLines() const
        {
            return serialLines;
        }

        const std::string& LastSerialLine() const
        {
            return lastSerialLine;
        }

        std::chrono::milliseconds LastSerialDuration() const
        {
            return lastSerialDuration;
        }

        void ClearCanFrames();

        bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
            std::chrono::milliseconds timeout);

        bool WaitForCanFrame(hal::Can::Id expectedId,
            CanFrame& outFrame,
            std::chrono::milliseconds timeout,
            std::chrono::steady_clock::time_point referenceTime,
            std::chrono::milliseconds& outElapsed);

    private:
        BridgeSession();

        void Connect();
        void RunUntil(const std::function<bool()>& predicate, std::chrono::milliseconds timeout);

        class Tracker
            : public tool::TcpClientObserver
        {
        public:
            explicit Tracker(tool::TcpClient& subject)
                : tool::TcpClientObserver(subject)
            {}

            void Connected() override
            {
                connectedOnce = true;
                alive = true;
            }

            void Disconnected() override
            {
                alive = false;
            }

            bool Alive() const
            {
                return alive;
            }

            bool ConnectedOnce() const
            {
                return connectedOnce;
            }

        private:
            bool connectedOnce{ false };
            bool alive{ false };
        };

        BridgeConfig config;

        main_::NetworkAdapter network;
        std::unique_ptr<tool::TcpClientSerial> serial;
        std::unique_ptr<tool::TcpClientCanbus> can;
        std::unique_ptr<Tracker> serialTracker;
        std::unique_ptr<Tracker> canTracker;

        std::string serialBuffer;
        std::vector<std::string> serialLines;
        std::string lastSerialLine;
        std::chrono::milliseconds lastSerialDuration{ 0 };

        std::deque<CanFrame> canQueue;
    };
}
