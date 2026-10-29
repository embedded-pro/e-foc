#include "targets/platform_implementations/st/implementation/FdCanStm.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>
#include <cstdlib>

namespace
{
    const std::array instances{
        FDCAN1,
#if defined(FDCAN2)
        FDCAN2,
#endif
#if defined(FDCAN3)
        FDCAN3,
#endif
    };

    constexpr std::array irqs{
        FDCAN1_IT0_IRQn,
#if defined(FDCAN2)
        FDCAN2_IT0_IRQn,
#endif
#if defined(FDCAN3)
        FDCAN3_IT0_IRQn,
#endif
    };

    constexpr uint32_t minimumTimeQuanta = 8;
    constexpr uint32_t maximumTimeQuanta = 25;
    constexpr uint32_t maximumPrescaler = 512;
    constexpr uint8_t maxDataLength = 8;
    constexpr uint32_t dlcShift = 16;

    constexpr uint32_t interrupts = FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO0_MESSAGE_LOST | FDCAN_IT_TX_COMPLETE | FDCAN_IT_ERROR_WARNING | FDCAN_IT_ERROR_PASSIVE | FDCAN_IT_BUS_OFF | FDCAN_IT_ARB_PROTOCOL_ERROR;
    constexpr uint32_t statusFlags = FDCAN_FLAG_ERROR_WARNING | FDCAN_FLAG_ERROR_PASSIVE | FDCAN_FLAG_BUS_OFF | FDCAN_FLAG_ARB_PROTOCOL_ERROR;
    constexpr uint32_t allTxBuffers = FDCAN_TX_BUFFER0 | FDCAN_TX_BUFFER1 | FDCAN_TX_BUFFER2;

    std::optional<hal::FdCanStm::Error> LastErrorCodeToError(uint32_t lastErrorCode)
    {
        switch (lastErrorCode)
        {
            case FDCAN_PROTOCOL_ERROR_STUFF:
                return hal::FdCanStm::Error::stuffError;
            case FDCAN_PROTOCOL_ERROR_FORM:
                return hal::FdCanStm::Error::formError;
            case FDCAN_PROTOCOL_ERROR_ACK:
                return hal::FdCanStm::Error::ackError;
            case FDCAN_PROTOCOL_ERROR_BIT1:
                return hal::FdCanStm::Error::bit1Error;
            case FDCAN_PROTOCOL_ERROR_BIT0:
                return hal::FdCanStm::Error::bit0Error;
            case FDCAN_PROTOCOL_ERROR_CRC:
                return hal::FdCanStm::Error::crcError;
            default:
                return std::nullopt;
        }
    }
}

namespace hal
{
    // Sample point near 87.5 %, the CiA recommendation; the most time quanta that divide the clock exactly win
    FdCanStm::NominalBitTiming FdCanStm::CalculateNominalBitTiming(uint32_t kernelClock, uint32_t bitRate)
    {
        really_assert(bitRate != 0);

        uint32_t bestQuanta = maximumTimeQuanta;
        uint32_t bestPrescaler = 1;
        uint32_t bestError = UINT32_MAX;

        for (uint32_t quanta = maximumTimeQuanta; quanta >= minimumTimeQuanta; --quanta)
        {
            const auto quantumRate = static_cast<uint64_t>(bitRate) * quanta;
            const auto prescaler = std::clamp<uint64_t>((kernelClock + quantumRate / 2) / quantumRate, 1, maximumPrescaler);
            const auto actual = kernelClock / (prescaler * quanta);
            const auto error = static_cast<uint32_t>(actual > bitRate ? actual - bitRate : bitRate - actual);

            if (error < bestError)
            {
                bestError = error;
                bestQuanta = quanta;
                bestPrescaler = static_cast<uint32_t>(prescaler);
            }

            if (error == 0)
                break;
        }

        const auto timeSegment2 = std::max<uint32_t>(1, bestQuanta / 8);
        return { bestPrescaler, bestQuanta - 1 - timeSegment2, timeSegment2, timeSegment2 };
    }

    FdCanStm::FdCanStm(infra::MemoryRange<FdCanRxEntry> rxStorage, uint8_t oneBasedIndex, GpioPinStm& rxPin, GpioPinStm& txPin, const Config& config, const infra::Function<void(Error)>& onError)
        : rxQueue(rxStorage, [this]()
              {
                  ProcessRxBuffer();
              })
        , index(static_cast<uint8_t>(oneBasedIndex - 1))
        , rx(rxPin, PinConfigTypeStm::canRx, oneBasedIndex)
        , tx(txPin, PinConfigTypeStm::canTx, oneBasedIndex)
        , config(config)
        , onError(onError)
    {
        really_assert(oneBasedIndex >= 1 && oneBasedIndex <= instances.size());

        __HAL_RCC_FDCAN_CLK_ENABLE();
        handle.Instance = instances[index];

        Initialize();

        NVIC_ClearPendingIRQ(irqs[index]);
        interruptHandler.emplace(irqs[index], this->config.interruptPriority, [this]()
            {
                HandleInterrupt();
            });
    }

    FdCanStm::~FdCanStm()
    {
        interruptHandler.reset();

        HAL_FDCAN_Stop(&handle);
        HAL_FDCAN_DeInit(&handle);
    }

    void FdCanStm::Initialize()
    {
        const auto timing = CalculateNominalBitTiming(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_FDCAN), config.bitRate);

        handle.Init.ClockDivider = FDCAN_CLOCK_DIV1;
        handle.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
        handle.Init.Mode = config.testMode ? FDCAN_MODE_EXTERNAL_LOOPBACK : FDCAN_MODE_NORMAL;
        handle.Init.AutoRetransmission = ENABLE;
        handle.Init.TransmitPause = DISABLE;
        handle.Init.ProtocolException = DISABLE;
        handle.Init.NominalPrescaler = timing.prescaler;
        handle.Init.NominalSyncJumpWidth = timing.syncJumpWidth;
        handle.Init.NominalTimeSeg1 = timing.timeSegment1;
        handle.Init.NominalTimeSeg2 = timing.timeSegment2;
        handle.Init.DataPrescaler = 1;
        handle.Init.DataSyncJumpWidth = 1;
        handle.Init.DataTimeSeg1 = 1;
        handle.Init.DataTimeSeg2 = 1;
        handle.Init.StdFiltersNbr = 0;
        handle.Init.ExtFiltersNbr = 0;
        handle.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;

        auto result = HAL_FDCAN_Init(&handle);
        really_assert(result == HAL_OK);

        result = HAL_FDCAN_ConfigGlobalFilter(&handle, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
        really_assert(result == HAL_OK);

        result = HAL_FDCAN_ActivateNotification(&handle, interrupts, allTxBuffers);
        really_assert(result == HAL_OK);

        result = HAL_FDCAN_Start(&handle);
        really_assert(result == HAL_OK);
    }

    void FdCanStm::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        really_assert(!sending.load(std::memory_order_acquire));
        really_assert(data.size() <= maxDataLength);

        onSendComplete = actionOnCompletion;
        sending.store(true, std::memory_order_release);

        FDCAN_TxHeaderTypeDef header{};
        header.Identifier = id.Is29BitId() ? id.Get29BitId() : id.Get11BitId();
        header.IdType = id.Is29BitId() ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
        header.TxFrameType = FDCAN_DATA_FRAME;
        header.DataLength = static_cast<uint32_t>(data.size()) << dlcShift;
        header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
        header.BitRateSwitch = FDCAN_BRS_OFF;
        header.FDFormat = FDCAN_CLASSIC_CAN;
        header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        header.MessageMarker = 0;

        std::array<uint8_t, maxDataLength> payload{};
        std::copy(data.begin(), data.end(), payload.begin());

        if (HAL_FDCAN_AddMessageToTxFifoQ(&handle, &header, payload.data()) != HAL_OK)
            NotifySendFailedFromInterrupt();
    }

    void FdCanStm::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        onReceive = receivedAction;
    }

    void FdCanStm::HandleInterrupt()
    {
        const auto flags = handle.Instance->IR & handle.Instance->IE;
        __HAL_FDCAN_CLEAR_FLAG(&handle, flags);

        if ((flags & FDCAN_FLAG_RX_FIFO0_NEW_MESSAGE) != 0)
            HandleRxInterrupt();

        if ((flags & FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST) != 0)
            ScheduleError(Error::messageLost);

        if ((flags & FDCAN_FLAG_TX_COMPLETE) != 0)
            HandleTxInterrupt();

        if ((flags & statusFlags) != 0)
            HandleStatusInterrupt();
    }

    void FdCanStm::HandleStatusInterrupt()
    {
        FDCAN_ProtocolStatusTypeDef status{};
        HAL_FDCAN_GetProtocolStatus(&handle, &status);

        if (auto error = LastErrorCodeToError(status.LastErrorCode))
            ScheduleError(*error);

        const bool warning = status.Warning != 0;
        if (warning && !previousWarning)
            ScheduleError(Error::errorWarning);
        previousWarning = warning;

        const bool passive = status.ErrorPassive != 0;
        if (passive && !previousPassive)
            ScheduleError(Error::errorPassive);
        previousPassive = passive;

        if (status.BusOff != 0)
        {
            ScheduleError(Error::busOff);

            // Bus-off sets CCCR.INIT; the frame is cancelled so recovery does not send it after its completion was reported
            handle.Instance->TXBCR = handle.Instance->TXBRP;
            NotifySendFailedFromInterrupt();

            if (config.autoBusOffRecovery)
                CLEAR_BIT(handle.Instance->CCCR, FDCAN_CCCR_INIT);
        }
    }

    void FdCanStm::HandleTxInterrupt()
    {
        if (sending.exchange(false, std::memory_order_acq_rel))
            QueueSendCompletion(true);
    }

    void FdCanStm::HandleRxInterrupt()
    {
        FDCAN_RxHeaderTypeDef header{};
        std::array<uint8_t, maxDataLength> payload{};

        while (HAL_FDCAN_GetRxFifoFillLevel(&handle, FDCAN_RX_FIFO0) != 0)
        {
            if (HAL_FDCAN_GetRxMessage(&handle, FDCAN_RX_FIFO0, &header, payload.data()) != HAL_OK)
                return;

            if (rxQueue.Full())
            {
                ScheduleError(Error::messageLost);
                continue;
            }

            FdCanRxEntry entry{};
            entry.is29Bit = header.IdType == FDCAN_EXTENDED_ID;
            entry.id = header.Identifier;
            entry.length = static_cast<uint8_t>(std::min<uint32_t>(header.DataLength >> dlcShift, maxDataLength));
            std::copy_n(payload.begin(), entry.length, entry.data.begin());
            rxQueue.AddFromInterrupt(entry);
        }
    }

    void FdCanStm::ProcessRxBuffer()
    {
        while (!rxQueue.Empty())
        {
            const FdCanRxEntry entry = rxQueue.Get();
            const Id id = entry.is29Bit ? Id::Create29BitId(entry.id) : Id::Create11BitId(entry.id);

            Message data;
            for (uint8_t i = 0; i != entry.length; ++i)
                data.push_back(entry.data[i]);

            if (onReceive)
                onReceive(id, data);
        }
    }

    // A bus nobody acknowledges reports an error per retransmission; one pending notification per class keeps the event queue bounded
    void FdCanStm::ScheduleError(Error error) const
    {
        const auto bit = static_cast<uint16_t>(1u << static_cast<uint8_t>(error));

        if ((pendingErrors.fetch_or(bit, std::memory_order_acq_rel) & bit) != 0)
            return;

        infra::EventDispatcher::Instance().Schedule([this, error, bit]()
            {
                pendingErrors.fetch_and(static_cast<uint16_t>(~bit), std::memory_order_acq_rel);

                if (onError)
                    onError(error);
            });
    }

    void FdCanStm::NotifySendFailedFromInterrupt()
    {
        if (sending.exchange(false, std::memory_order_acq_rel))
            QueueSendCompletion(false);
    }

    // The callback is copied here because the next SendData may rebind onSendComplete before the dispatcher runs the completion
    void FdCanStm::QueueSendCompletion(bool success)
    {
        const auto end = sendCompletionsEnd.load(std::memory_order_relaxed);
        really_assert(static_cast<uint8_t>(end - sendCompletionsBegin.load(std::memory_order_acquire)) < sendCompletions.size());

        sendCompletions[end % sendCompletions.size()] = SendCompletion{ onSendComplete, success };
        sendCompletionsEnd.store(static_cast<uint8_t>(end + 1), std::memory_order_release);

        infra::EventDispatcher::Instance().Schedule([this]()
            {
                DispatchSendCompletion();
            });
    }

    void FdCanStm::DispatchSendCompletion()
    {
        const auto begin = sendCompletionsBegin.load(std::memory_order_relaxed);
        const SendCompletion completion = sendCompletions[begin % sendCompletions.size()];
        sendCompletionsBegin.store(static_cast<uint8_t>(begin + 1), std::memory_order_release);

        if (completion.action)
            completion.action(completion.success);
    }
}
