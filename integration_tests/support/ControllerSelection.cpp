#include "integration_tests/support/ControllerSelection.hpp"
#include <array>
#include <chrono>

namespace integration
{
    namespace
    {
        constexpr auto kSelectionTimeout = std::chrono::seconds{ 10 };

        template<std::size_t Size>
        std::optional<uint8_t> IndexOf(const std::array<const char*, Size>& names, const std::string& name)
        {
            for (uint8_t index = 0; index != names.size(); ++index)
                if (name == names[index])
                    return index;
            return std::nullopt;
        }
    }

    std::optional<can::FocControlLoop> LoopNamed(const std::string& name)
    {
        if (name == "current")
            return can::FocControlLoop::current;
        if (name == "speed")
            return can::FocControlLoop::speed;
        if (name == "position")
            return can::FocControlLoop::position;
        return std::nullopt;
    }

    std::optional<uint8_t> AlgorithmNamed(can::FocControlLoop loop, const std::string& name)
    {
        static constexpr std::array<const char*, 4> current{ "pid", "decoupled", "deadbeat", "sliding" };
        static constexpr std::array<const char*, 4> speed{ "pid", "lqi", "adrc", "twodof" };
        static constexpr std::array<const char*, 5> position{ "pid", "cascadep", "lqr", "lqi", "twodof" };

        if (loop == can::FocControlLoop::current)
            return IndexOf(current, name);
        if (loop == can::FocControlLoop::speed)
            return IndexOf(speed, name);
        return IndexOf(position, name);
    }

    bool SelectAlgorithmOverCan(Fixture& fixture, can::FocControlLoop loop, uint8_t algorithm)
    {
        hal::Can::Message payload;
        payload.push_back(static_cast<uint8_t>(loop));
        payload.push_back(algorithm);
        return fixture.SendCanCommand(can::focMotorCategoryId, can::focSelectAlgorithmId, payload, kSelectionTimeout);
    }
}
