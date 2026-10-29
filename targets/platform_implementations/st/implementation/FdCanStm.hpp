#pragma once

#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/interfaces/Can.hpp"
#include "hal_st/stm32fxxx/GpioStm.hpp"
#include "infra/event/QueueForOneReaderOneIrqWriter.hpp"
#include "infra/util/WithStorage.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include DEVICE_HEADER

namespace hal
{
    struct FdCanRxEntry
    {
        uint32_t id;
        std::array<uint8_t, 8> data;
        uint8_t length;
        bool is29Bit;
    };

    // Classic CAN frames on an FDCAN instance; like hal::Can, one frame is in flight at a time
    class FdCanStm
        : public hal::Can
    {
    public:
        template<std::size_t StorageSize>
        using WithMaxRxBuffer = infra::WithStorage<FdCanStm, std::array<FdCanRxEntry, StorageSize + 1>>;

        enum class Error : uint8_t
        {
            stuffError,
            formError,
            ackError,
            bit1Error,
            bit0Error,
            crcError,
            errorWarning,
            errorPassive,
            busOff,
            messageLost,
        };

        struct Config
        {
            constexpr Config()
            {}

            uint32_t bitRate{ 1'000'000 };
            bool testMode{ false };
            bool autoBusOffRecovery{ true };
            cortex::InterruptPriority interruptPriority{ cortex::InterruptPriority::normal };
        };

        struct NominalBitTiming
        {
            uint32_t prescaler;
            uint32_t timeSegment1;
            uint32_t timeSegment2;
            uint32_t syncJumpWidth;
        };

        static NominalBitTiming CalculateNominalBitTiming(uint32_t kernelClock, uint32_t bitRate);

        FdCanStm(infra::MemoryRange<FdCanRxEntry> rxStorage, uint8_t oneBasedIndex, GpioPinStm& rx, GpioPinStm& tx, const Config& config, const infra::Function<void(Error)>& onError);
        FdCanStm(const FdCanStm&) = delete;
        FdCanStm& operator=(const FdCanStm&) = delete;
        ~FdCanStm();

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

    private:
        struct SendCompletion
        {
            infra::Function<void(bool success)> action;
            bool success{ false };
        };

        void Initialize();
        void HandleInterrupt();
        void HandleStatusInterrupt();
        void HandleTxInterrupt();
        void HandleRxInterrupt();

        void ProcessRxBuffer();
        void ScheduleError(Error error) const;
        void NotifySendFailedFromInterrupt();
        void QueueSendCompletion(bool success);
        void DispatchSendCompletion();

    private:
        infra::QueueForOneReaderOneIrqWriter<FdCanRxEntry> rxQueue;
        uint8_t index;
        PeripheralPinStm rx;
        PeripheralPinStm tx;
        Config config;
        FDCAN_HandleTypeDef handle{};
        infra::Function<void(bool success)> onSendComplete;
        infra::Function<void(Id id, const Message& data)> onReceive;
        infra::Function<void(Error)> onError;
        std::atomic<bool> sending{ false };
        mutable std::atomic<uint16_t> pendingErrors{ 0 };
        std::array<SendCompletion, 2> sendCompletions;
        std::atomic<uint8_t> sendCompletionsBegin{ 0 };
        std::atomic<uint8_t> sendCompletionsEnd{ 0 };
        bool previousWarning{ false };
        bool previousPassive{ false };
        std::optional<cortex::ImmediateInterruptHandler> interruptHandler;
    };
}
