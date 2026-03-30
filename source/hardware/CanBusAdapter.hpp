#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/stream/OutputStream.hpp"
#include "infra/util/Function.hpp"
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace application
{
    class CanBusAdapter
        : public hal::Can
    {
    public:
        enum class CanError : uint8_t
        {
            busOff,
            errorPassive,
            errorWarning,
            messageLost,
            rxBufferOverflow,
            other,
        };

        virtual void SetOnError(const infra::Function<void(CanError)>& handler) = 0;
    };

    template<std::derived_from<hal::Can> Impl>
    class CanBusAdapterImpl
        : public CanBusAdapter
    {
    public:
        CanBusAdapterImpl() = default;

        template<typename... Args>
        requires(sizeof...(Args) > 0 && (!std::same_as<std::remove_cvref_t<Args>, CanBusAdapterImpl> && ...))
        explicit CanBusAdapterImpl(Args&&... args);

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;
        void SetOnError(const infra::Function<void(CanError)>& handler) override;
        void InvokeErrorHandler(CanError error);

    private:
        Impl can;
        infra::Function<void(CanError)> onError;
    };

    // Implementation

    template<std::derived_from<hal::Can> Impl>
    template<typename... Args>
    requires(sizeof...(Args) > 0 && (!std::same_as<std::remove_cvref_t<Args>, CanBusAdapterImpl<Impl>> && ...))
    CanBusAdapterImpl<Impl>::CanBusAdapterImpl(Args&&... args)
        : can(std::forward<Args>(args)...)
    {
    }

    template<std::derived_from<hal::Can> Impl>
    void CanBusAdapterImpl<Impl>::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        can.SendData(id, data, actionOnCompletion);
    }

    template<std::derived_from<hal::Can> Impl>
    void CanBusAdapterImpl<Impl>::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        can.ReceiveData(receivedAction);
    }

    template<std::derived_from<hal::Can> Impl>
    void CanBusAdapterImpl<Impl>::SetOnError(const infra::Function<void(CanError)>& handler)
    {
        onError = handler;
    }

    template<std::derived_from<hal::Can> Impl>
    void CanBusAdapterImpl<Impl>::InvokeErrorHandler(CanError error)
    {
        if (onError)
            onError(error);
    }
}

namespace infra
{
    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, const application::CanBusAdapter::CanError& error);
}
