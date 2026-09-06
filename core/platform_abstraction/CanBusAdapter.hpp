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
            void Record(CanError error)
            {
                auto& counter = counters.at(IndexOf(error));
                counter = Increment(counter);
                total = Increment(total);
            }

            void Reset()
            {
                counters = std::array<uint32_t, errorClasses>{};
                total = 0;
            }

            uint32_t Count(CanError error) const
            {
                return counters.at(IndexOf(error));
            }

            uint32_t Total() const
            {
                return total;
            }

        private:
            static constexpr uint32_t saturated = 0xFFFFFFFFu;

            static std::size_t IndexOf(CanError error)
            {
                const auto index = static_cast<std::size_t>(error);

                return index < errorClasses ? index : errorClasses - 1;
            }

            static uint32_t Increment(uint32_t counter)
            {
                return counter == saturated ? counter : counter + 1;
            }

            std::array<uint32_t, errorClasses> counters{};
            uint32_t total{ 0 };
        };

        virtual void SetOnError(const infra::Function<void(CanError)>& handler) = 0;

        const ErrorCounters& ErrorStatistics() const
        {
            return errorCounters;
        }

        void ResetErrorStatistics()
        {
            errorCounters.Reset();
        }

    protected:
        void RecordError(CanError error)
        {
            errorCounters.Record(error);
        }

    private:
        ErrorCounters errorCounters;

    public:
        friend infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, CanError error)
        {
            using enum CanError;
            switch (error)
            {
                case busOff:
                    stream << "bus off";
                    break;
                case errorPassive:
                    stream << "error passive";
                    break;
                case errorWarning:
                    stream << "error warning";
                    break;
                case messageLost:
                    stream << "message lost";
                    break;
                case rxBufferOverflow:
                    stream << "rx buffer overflow";
                    break;
                case ackError:
                    stream << "ack error";
                    break;
                case stuffError:
                    stream << "stuff error";
                    break;
                case formError:
                    stream << "form error";
                    break;
                case crcError:
                    stream << "crc error";
                    break;
                case bit0Error:
                    stream << "bit0 error";
                    break;
                case bit1Error:
                    stream << "bit1 error";
                    break;
                default:
                    stream << "unknown";
                    break;
            }

            return stream;
        }
    };

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
