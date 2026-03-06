#pragma once

#include "infra/util/BoundedString.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace tool
{
    using CanFrame = infra::BoundedVector<uint8_t>::WithMaxSize<8>;

    class CanBusAdapter;

    class CanBusAdapterObserver
        : public infra::SingleObserver<CanBusAdapterObserver, CanBusAdapter>
    {
    public:
        using infra::SingleObserver<CanBusAdapterObserver, CanBusAdapter>::SingleObserver;

        virtual void OnFrameReceived(uint32_t id, const CanFrame& data) = 0;
        virtual void OnError(infra::BoundedConstString message) = 0;
        virtual void OnConnectionChanged(bool connected) = 0;
    };

    class CanBusAdapter
        : public infra::Subject<CanBusAdapterObserver>
    {
    public:
        CanBusAdapter() = default;
        virtual ~CanBusAdapter() = default;

        CanBusAdapter(const CanBusAdapter&) = delete;
        CanBusAdapter& operator=(const CanBusAdapter&) = delete;

        virtual bool Connect(infra::BoundedConstString interfaceName, uint32_t bitrate) = 0;
        virtual void Disconnect() = 0;
        virtual bool IsConnected() const = 0;
        virtual bool Send(uint32_t id, const CanFrame& data) = 0;

        virtual int FileDescriptor() const = 0;
        virtual void ProcessReadEvent() = 0;
    };
}
