#pragma once

#include "can-lite/drivers/implementation/LoopbackSocket.hpp"
#include "tools/virtual_motor/app/logic/MotorLink.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace virtual_motor::app
{
    class PortListener
    {
    public:
        PortListener() = default;
        PortListener(const PortListener&) = delete;
        PortListener& operator=(const PortListener&) = delete;
        ~PortListener();

        bool Listen();
        std::optional<uint16_t> Port() const;
        void Poll();
        void OnIntroduced(const std::function<void(std::unique_ptr<MotorLink>)>& callback);

    private:
        std::function<void(std::unique_ptr<MotorLink>)> onIntroduced;
        intptr_t listener{ services::loopback::invalidSocket };
        std::vector<std::unique_ptr<MotorLink>> pending;
    };
}
