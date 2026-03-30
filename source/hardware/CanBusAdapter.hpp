#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/Function.hpp"
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

    template<typename Impl, typename = std::enable_if_t<std::is_base_of_v<hal::Can, Impl>>>
    class CanBusAdapterImpl
        : public CanBusAdapter
    {
    public:
        template<typename... Args>
        explicit CanBusAdapterImpl(Args&&... args);

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;
        void SetOnError(const infra::Function<void(CanError)>& handler) override;

    private:
        Impl can;
        infra::Function<void(CanError)> onError;
    };

    // Implementation

    template<typename Impl, typename Enable>
    template<typename... Args>
    CanBusAdapterImpl<Impl, Enable>::CanBusAdapterImpl(Args&&... args)
        : can(std::forward<Args>(args)...)
    {
    }

    template<typename Impl, typename Enable>
    void CanBusAdapterImpl<Impl, Enable>::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        can.SendData(id, data, actionOnCompletion);
    }

    template<typename Impl, typename Enable>
    void CanBusAdapterImpl<Impl, Enable>::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        can.ReceiveData(receivedAction);
    }

    template<typename Impl, typename Enable>
    void CanBusAdapterImpl<Impl, Enable>::SetOnError(const infra::Function<void(CanError)>& handler)
    {
        onError = handler;
    }
}
