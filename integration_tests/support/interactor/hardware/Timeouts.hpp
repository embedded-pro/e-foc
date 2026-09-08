#pragma once

#include <chrono>

namespace hil
{
    namespace timeouts
    {
        inline constexpr auto idleWindow = std::chrono::milliseconds{ 75 };
        inline constexpr auto preDrain = std::chrono::milliseconds{ 30 };

        inline constexpr auto command = std::chrono::milliseconds{ 500 };
        inline constexpr auto slowCommand = std::chrono::milliseconds{ 2000 };
        inline constexpr auto asyncCommand = std::chrono::milliseconds{ 3000 };

        inline constexpr auto prompt = std::chrono::milliseconds{ 1000 };
        inline constexpr auto promptShort = std::chrono::milliseconds{ 500 };
        inline constexpr auto cleanup = std::chrono::milliseconds{ 200 };

        inline constexpr auto bootBanner = std::chrono::milliseconds{ 5000 };
        inline constexpr auto reconnectPoll = std::chrono::milliseconds{ 200 };

        inline constexpr auto typingInterCharDelay = std::chrono::milliseconds{ 5 };
    }
}
