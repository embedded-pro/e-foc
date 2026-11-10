#include "tools/virtual_motor/platform/VirtualCanPort.hpp"

namespace virtual_motor
{
    VirtualCanPort::VirtualCanPort(uint16_t hubPort)
        : port(hubPort)
    {}

    bool VirtualCanPort::Connect(infra::BoundedConstString bus, uint32_t bitrate)
    {
        return adapter.Connect(bus, bitrate);
    }

    bool VirtualCanPort::IsConnected() const
    {
        return adapter.IsConnected();
    }

    void VirtualCanPort::Poll()
    {
        adapter.ProcessReadEvent();
    }

    void VirtualCanPort::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        adapter.SendData(id, data, actionOnCompletion);
    }

    void VirtualCanPort::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        adapter.ReceiveData(receivedAction);
    }

    void VirtualCanPort::SetOnError(const infra::Function<void(services::CanBusError)>& handler)
    {
        onError = handler;
    }

    void VirtualCanPort::ReportError()
    {
        RecordError(services::CanBusError::other);
        if (onError)
            onError(services::CanBusError::other);
    }

    VirtualCanPort::ErrorObserver::ErrorObserver(services::CanBusAdapter& adapter, VirtualCanPort& owner)
        : services::CanBusAdapterObserver(adapter)
        , owner(owner)
    {}

    void VirtualCanPort::ErrorObserver::OnFrameLog(bool, uint32_t, const services::CanFrame&)
    {}

    void VirtualCanPort::ErrorObserver::OnError(infra::BoundedConstString)
    {
        owner.ReportError();
    }

    void VirtualCanPort::ErrorObserver::OnConnectionChanged(bool)
    {}
}
