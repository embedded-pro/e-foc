#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/stream/OutputStream.hpp"
#include "infra/util/Function.hpp"
#include <array>
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
        virtual ~CanBusAdapter() = default;
        enum class CanError : uint8_t
        {
            busOff,
            errorPassive,
            errorWarning,
            messageLost,
            rxBufferOverflow,
            ackError,
            stuffError,
            formError,
            crcError,
            bit0Error,
            bit1Error,
            other,
        };

        static constexpr std::size_t errorClasses = static_cast<std::size_t>(CanError::other) + 1;

        class ErrorCounters
        {
        public:
            void Record(CanError error);
            void Reset();
            uint32_t Count(CanError error) const;
            uint32_t Total() const;

        private:
            static constexpr uint32_t saturated = 0xFFFFFFFFu;

            static std::size_t IndexOf(CanError error);
            static uint32_t Increment(uint32_t counter);

            std::array<uint32_t, errorClasses> counters{};
            uint32_t total{ 0 };
        };

        virtual void SetOnError(const infra::Function<void(CanError)>& handler) = 0;

        const ErrorCounters& ErrorStatistics() const;
        void ResetErrorStatistics();

    protected:
        void RecordError(CanError error);

    private:
        ErrorCounters errorCounters;
    };

    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, CanBusAdapter::CanError error);

    template<std::derived_from<hal::Can> Impl>
    class CanBusAdapterImpl
        : public CanBusAdapter
    {
    public:
        CanBusAdapterImpl() = default;

        template<typename... Args>
        requires(sizeof...(Args) > 0 && (!std::same_as<std::remove_cvref_t<Args>, CanBusAdapterImpl> && ...))
        explicit CanBusAdapterImpl(Args&&... args)
            : can(std::forward<Args>(args)...)
        {}

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
        RecordError(error);

        if (onError)
            onError(error);
    }
}
