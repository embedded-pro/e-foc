#include "tools/virtual_motor/app/logic/PortListener.hpp"
#include "can-lite/drivers/implementation/LoopbackSocket.hpp"
#include <algorithm>

namespace virtual_motor::app
{
    PortListener::~PortListener()
    {
        services::loopback::Close(listener);
    }

    bool PortListener::Listen()
    {
        listener = services::loopback::Listen(0);
        return listener != services::loopback::invalidSocket;
    }

    std::optional<uint16_t> PortListener::Port() const
    {
        if (listener == services::loopback::invalidSocket)
            return std::nullopt;
        return services::loopback::LocalPort(listener);
    }

    void PortListener::OnIntroduced(const std::function<void(std::unique_ptr<MotorLink>)>& callback)
    {
        onIntroduced = callback;
    }

    void PortListener::Poll()
    {
        if (listener == services::loopback::invalidSocket)
            return;

        for (auto socket = services::loopback::Accept(listener); socket != services::loopback::invalidSocket; socket = services::loopback::Accept(listener))
            pending.push_back(std::make_unique<MotorLink>(socket));

        for (auto& link : pending)
        {
            link->Poll();
            if (link->Identity().has_value() && onIntroduced)
                onIntroduced(std::move(link));
        }

        std::erase_if(pending, [](const auto& link)
            {
                return link == nullptr || !link->IsOpen();
            });
    }
}
