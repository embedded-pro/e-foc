#pragma once

#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>
#include <map>

namespace tool
{
    class CommandTracker
    {
    public:
        static constexpr infra::Duration commandTimeout{ std::chrono::seconds(2) };
        static constexpr infra::Duration startCommandTimeout{ std::chrono::seconds(12) };
        static constexpr infra::Duration motionCommandTimeout{ std::chrono::minutes(2) };

        CommandTracker(const infra::Function<void(uint16_t nodeId, bool busy)>& onBusyChanged, const infra::Function<void(uint16_t nodeId, uint8_t commandType)>& onTimeout);

        static infra::Duration TimeoutFor(uint8_t commandType);

        bool IsBusy(uint16_t nodeId) const;
        void MarkBusy(uint16_t nodeId, uint8_t commandType);
        void Clear(uint16_t nodeId);
        void Complete(uint16_t nodeId, uint8_t commandType);
        void ClearAll();

    private:
        struct NodeCommand
        {
            bool busy{ false };
            uint8_t pendingCommand{ 0 };
            infra::TimerSingleShot timeout;
        };

        infra::Function<void(uint16_t, bool)> onBusyChanged;
        infra::Function<void(uint16_t, uint8_t)> onTimeout;
        std::map<uint16_t, NodeCommand> commands;
    };
}
