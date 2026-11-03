#include "integration_tests/support/FirmwareUpgradeClient.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "infra/util/Crc.hpp"
#include <algorithm>

namespace integration
{
    namespace
    {
        constexpr std::size_t blockSize{ 6 };
        constexpr uint8_t blockAttempts{ 4 };
        constexpr auto sendTimeout = std::chrono::milliseconds{ 100 };
        constexpr auto beginTimeout = std::chrono::seconds{ 20 };
        constexpr auto blockTimeout = std::chrono::milliseconds{ 1000 };
        constexpr auto verifyTimeout = std::chrono::seconds{ 20 };
        constexpr auto answerTimeout = std::chrono::seconds{ 2 };

        hal::Can::Id MakeId(services::CanPriority priority, uint8_t category, uint8_t messageType, uint16_t nodeId)
        {
            return hal::Can::Id::Create29BitId(services::MakeCanId(priority, category, messageType, nodeId));
        }

        uint16_t ReadUInt16(const hal::Can::Message& message, std::size_t offset)
        {
            return static_cast<uint16_t>((message[offset] << 8) | message[offset + 1]);
        }
    }

    FirmwareUpgradeClient::FirmwareUpgradeClient(Fixture& fixture)
        : fixture(fixture)
    {}

    std::optional<FirmwareUpgradeClient::BeginAnswer> FirmwareUpgradeClient::Begin(uint32_t size)
    {
        hal::Can::Message payload;
        for (int shift = 24; shift >= 0; shift -= 8)
            payload.push_back(static_cast<uint8_t>(size >> shift));

        const auto answer = Exchange(services::fwuBeginUpgradeId, payload, services::fwuBeginResponseId, beginTimeout);
        if (!answer || answer->size() < 3)
            return std::nullopt;

        return BeginAnswer{ static_cast<services::FwuError>((*answer)[0]), ReadUInt16(*answer, 1) };
    }

    std::optional<services::FwuError> FirmwareUpgradeClient::Block(const std::vector<uint8_t>& pack, uint16_t index)
    {
        hal::Can::Message payload;
        payload.push_back(static_cast<uint8_t>(index >> 8));
        payload.push_back(static_cast<uint8_t>(index));
        const auto offset = static_cast<std::size_t>(index) * blockSize;
        for (std::size_t i = offset; i != std::min(offset + blockSize, pack.size()); ++i)
            payload.push_back(pack[i]);

        for (uint8_t attempt = 0; attempt != blockAttempts; ++attempt)
        {
            const auto answer = Exchange(services::fwuDataBlockId, payload, services::fwuDataBlockAckId, blockTimeout);
            if (answer && answer->size() >= 3 && ReadUInt16(*answer, 1) == index)
                return static_cast<services::FwuError>((*answer)[0]);
        }

        return std::nullopt;
    }

    std::optional<services::FwuError> FirmwareUpgradeClient::Blocks(const std::vector<uint8_t>& pack, uint16_t first, uint16_t count)
    {
        for (uint16_t index = first; index != first + count; ++index)
        {
            const auto result = Block(pack, index);
            if (result != services::FwuError::ok)
                return result;
        }

        return services::FwuError::ok;
    }

    std::optional<services::FwuError> FirmwareUpgradeClient::Verify(uint32_t crc)
    {
        hal::Can::Message payload;
        for (int shift = 24; shift >= 0; shift -= 8)
            payload.push_back(static_cast<uint8_t>(crc >> shift));

        return StatusOf(Exchange(services::fwuVerifyId, payload, services::fwuVerifyResponseId, verifyTimeout));
    }

    std::optional<services::FwuError> FirmwareUpgradeClient::Activate()
    {
        return StatusOf(Exchange(services::fwuActivateId, {}, services::fwuActivateResponseId, answerTimeout));
    }

    bool FirmwareUpgradeClient::Abort()
    {
        const auto node = fixture.ServerNodeId();
        const auto ackId = MakeId(services::CanPriority::response, services::canSystemCategoryId, services::canCommandAckMessageTypeId, node);
        const auto mark = fixture.MarkCapture();
        if (!fixture.SendCanFrame(MakeId(services::CanPriority::command, services::firmwareUpgradeCategoryId, services::fwuAbortId, node), {}, sendTimeout))
            return false;

        const auto acks = fixture.CollectCapturedCanFrames(ackId, mark, 4, answerTimeout);
        return std::ranges::any_of(acks, [](const hal::Can::Message& ack)
            {
                return ack.size() >= 3 && ack[0] == services::firmwareUpgradeCategoryId && ack[1] == services::fwuAbortId && ack[2] == static_cast<uint8_t>(services::CanAckStatus::success);
            });
    }

    uint16_t FirmwareUpgradeClient::NumberOfBlocks(const std::vector<uint8_t>& pack)
    {
        return static_cast<uint16_t>((pack.size() + blockSize - 1) / blockSize);
    }

    uint32_t FirmwareUpgradeClient::Crc(const std::vector<uint8_t>& pack)
    {
        infra::Crc32 crc;
        crc.Update(infra::ConstByteRange{ pack.data(), pack.data() + pack.size() });
        return crc.Result();
    }

    std::optional<hal::Can::Message> FirmwareUpgradeClient::Exchange(uint8_t command, const hal::Can::Message& payload, uint8_t response, std::chrono::milliseconds timeout)
    {
        KeepClientAlive();

        const auto node = fixture.ServerNodeId();
        const auto mark = fixture.MarkCapture();
        if (!fixture.SendCanFrame(MakeId(services::CanPriority::command, services::firmwareUpgradeCategoryId, command, node), payload, sendTimeout))
            return std::nullopt;

        return fixture.WaitForCapturedCanFrame(MakeId(services::CanPriority::response, services::firmwareUpgradeCategoryId, response, node), mark, timeout);
    }

    std::optional<services::FwuError> FirmwareUpgradeClient::StatusOf(const std::optional<hal::Can::Message>& answer) const
    {
        if (!answer || answer->empty())
            return std::nullopt;

        return static_cast<services::FwuError>((*answer)[0]);
    }

    void FirmwareUpgradeClient::KeepClientAlive()
    {
        const auto now = std::chrono::steady_clock::now();
        if (now - lastHeartbeat < Fixture::kClientHeartbeatInterval)
            return;

        fixture.SendClientHeartbeat();
        lastHeartbeat = now;
    }
}
