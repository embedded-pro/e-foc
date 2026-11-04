#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeDefinitions.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/Crc.hpp"
#include "tools/can_commander/logic/FirmwareTransfer.hpp"
#include "tools/can_commander/logic/test/CanBusAdapterMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <vector>

namespace
{
    using namespace tool;
    using services::FwuError;
    using testing::_;
    using testing::AnyNumber;
    using testing::Invoke;
    using testing::StrictMock;

    class FirmwareTransferObserverMock
        : public FirmwareTransferObserver
    {
    public:
        using FirmwareTransferObserver::FirmwareTransferObserver;

        MOCK_METHOD(void, OnTransferProgress, (uint16_t nodeId, const TransferProgress& progress), (override));
        MOCK_METHOD(void, OnDownloadFinished, (uint16_t nodeId, DownloadOutcome outcome, services::FwuError error), (override));
    };

    struct Sent
    {
        uint16_t node;
        uint8_t category;
        uint8_t messageType;
        std::vector<uint8_t> payload;
    };

    class TestFirmwareTransfer
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        struct Expectations
        {
            explicit Expectations(TestFirmwareTransfer& test)
            {
                EXPECT_CALL(test.adapter, ReceiveData(_)).Times(2).WillRepeatedly([&test](const auto& callback)
                    {
                        test.receive = callback;
                    });
                EXPECT_CALL(test.adapter, SendData(_, _, _)).Times(AnyNumber()).WillRepeatedly(Invoke([&test](hal::Can::Id id, const hal::Can::Message& message, const infra::Function<void(bool)>& done)
                    {
                        const auto raw = id.Get29BitId();
                        test.sent.push_back({ static_cast<uint16_t>(services::ExtractCanNodeId(raw)), services::ExtractCanCategory(raw), services::ExtractCanMessageType(raw), { message.begin(), message.end() } });
                        done(true);
                    }));
            }
        };

        TestFirmwareTransfer()
        {
            EXPECT_CALL(observer, OnTransferProgress(_, _)).Times(AnyNumber()).WillRepeatedly(Invoke([this](uint16_t, const TransferProgress& progress)
                {
                    progresses.push_back(progress);
                }));
        }

        static InspectedPack Pack(std::size_t size)
        {
            InspectedPack pack;
            for (std::size_t i = 0; i != size; ++i)
                pack.bytes.push_back(static_cast<uint8_t>(i * 5 + 1));
            infra::Crc32 crc;
            crc.Update(infra::MakeRange(pack.bytes));
            pack.crc32 = crc.Result();
            pack.version = { 1, 7, 0 };
            return pack;
        }

        void Receive(uint8_t category, uint8_t messageType, uint16_t node, std::initializer_list<uint8_t> bytes)
        {
            hal::Can::Message message;
            for (auto byte : bytes)
                message.push_back(byte);
            receive(hal::Can::Id::Create29BitId(services::MakeCanId(services::CanPriority::response, category, messageType, node)), message);
        }

        void Fwu(uint8_t messageType, std::initializer_list<uint8_t> bytes, uint16_t node = 1)
        {
            Receive(services::firmwareUpgradeCategoryId, messageType, node, bytes);
        }

        void BeginAnswered(FwuError error = FwuError::ok, uint16_t node = 1)
        {
            Fwu(services::fwuBeginResponseId, { static_cast<uint8_t>(error), 0x01, 0x00 }, node);
        }

        void BlockAcked(uint16_t index, FwuError error = FwuError::ok)
        {
            Fwu(services::fwuDataBlockAckId, { static_cast<uint8_t>(error), static_cast<uint8_t>(index >> 8), static_cast<uint8_t>(index) });
        }

        void VerifyAnswered(FwuError error = FwuError::ok)
        {
            Fwu(services::fwuVerifyResponseId, { static_cast<uint8_t>(error) });
        }

        void ProgressAnswered(services::FwuState state, uint16_t received, uint16_t total)
        {
            Fwu(services::fwuProgressResponseId, { static_cast<uint8_t>(state), static_cast<uint8_t>(received >> 8), static_cast<uint8_t>(received), static_cast<uint8_t>(total >> 8), static_cast<uint8_t>(total) });
        }

        std::vector<Sent> SentUpgradeFrames() const
        {
            std::vector<Sent> frames;
            std::ranges::copy_if(sent, std::back_inserter(frames), [](const Sent& frame)
                {
                    return frame.category == services::firmwareUpgradeCategoryId;
                });
            return frames;
        }

        Sent LastUpgradeFrame() const
        {
            return SentUpgradeFrames().back();
        }

        void DownloadUntilVerify(const InspectedPack& pack)
        {
            ASSERT_TRUE(transfer.StartDownload(1, pack));
            BeginAnswered();
            for (uint16_t index = 0; index * FirmwareTransfer::blockSize < pack.bytes.size(); ++index)
                BlockAcked(index);
        }

        StrictMock<CanBusAdapterMock> adapter;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receive;
        std::vector<Sent> sent;
        Expectations expectations{ *this };
        CanCommandClient client{ adapter };
        FirmwareTransfer transfer{ client };
        StrictMock<FirmwareTransferObserverMock> observer{ transfer };
        std::vector<TransferProgress> progresses;
    };
}

TEST_F(TestFirmwareTransfer, a_download_sends_begin_then_one_block_at_a_time_then_verify_with_the_crc)
{
    const auto pack = Pack(20);

    ASSERT_TRUE(transfer.StartDownload(1, pack));
    EXPECT_EQ(services::fwuBeginUpgradeId, LastUpgradeFrame().messageType);
    EXPECT_EQ((std::vector<uint8_t>{ 0, 0, 0, 20 }), LastUpgradeFrame().payload);
    EXPECT_EQ(TransferPhase::erasing, progresses.back().phase);

    BeginAnswered();
    EXPECT_EQ((std::vector<uint8_t>{ 0, 0, 1, 6, 11, 16, 21, 26 }), LastUpgradeFrame().payload);
    BlockAcked(0);
    BlockAcked(1);
    BlockAcked(2);
    EXPECT_EQ((std::vector<uint8_t>{ 0, 3, pack.bytes[18], pack.bytes[19] }), LastUpgradeFrame().payload);
    BlockAcked(3);

    EXPECT_EQ(services::fwuVerifyId, LastUpgradeFrame().messageType);
    EXPECT_EQ((std::vector<uint8_t>{ static_cast<uint8_t>(pack.crc32 >> 24), static_cast<uint8_t>(pack.crc32 >> 16), static_cast<uint8_t>(pack.crc32 >> 8), static_cast<uint8_t>(pack.crc32) }), LastUpgradeFrame().payload);
    EXPECT_EQ((TransferProgress{ TransferPhase::verifying, 20, 20 }), progresses.back());

    EXPECT_CALL(observer, OnDownloadFinished(1, DownloadOutcome::staged, FwuError::ok));
    VerifyAnswered();
    EXPECT_FALSE(transfer.IsActive());
}

TEST_F(TestFirmwareTransfer, progress_counts_the_bytes_acknowledged)
{
    ASSERT_TRUE(transfer.StartDownload(1, Pack(20)));
    BeginAnswered();
    BlockAcked(0);

    EXPECT_EQ((TransferProgress{ TransferPhase::sending, 6, 20 }), progresses.back());
}

TEST_F(TestFirmwareTransfer, only_one_download_runs_on_the_bus)
{
    ASSERT_TRUE(transfer.StartDownload(1, Pack(20)));

    EXPECT_FALSE(transfer.StartDownload(2, Pack(20)));
    EXPECT_TRUE(transfer.IsActive());
    EXPECT_EQ(1, transfer.ActiveNode());
}

TEST_F(TestFirmwareTransfer, an_answer_from_another_node_is_ignored)
{
    ASSERT_TRUE(transfer.StartDownload(1, Pack(20)));
    const auto before = SentUpgradeFrames().size();

    BeginAnswered(FwuError::ok, 2);

    EXPECT_EQ(before, SentUpgradeFrames().size());
    EXPECT_EQ(TransferPhase::erasing, transfer.Phase());
}

TEST_F(TestFirmwareTransfer, begin_refused_ends_the_download_with_the_refusal)
{
    ASSERT_TRUE(transfer.StartDownload(1, Pack(20)));

    EXPECT_CALL(observer, OnDownloadFinished(1, DownloadOutcome::refused, FwuError::busy));
    BeginAnswered(FwuError::busy);
}

TEST_F(TestFirmwareTransfer, begin_unanswered_for_ten_seconds_gives_up_and_aborts)
{
    ASSERT_TRUE(transfer.StartDownload(1, Pack(20)));
    ForwardTime(FirmwareTransfer::beginTimeout - std::chrono::milliseconds(1));

    EXPECT_CALL(observer, OnDownloadFinished(1, DownloadOutcome::noAnswer, FwuError::ok));
    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_EQ(services::fwuAbortId, LastUpgradeFrame().messageType);
}

TEST_F(TestFirmwareTransfer, a_block_unanswered_for_a_second_asks_for_progress_and_resends_from_where_the_controller_stands)
{
    ASSERT_TRUE(transfer.StartDownload(1, Pack(20)));
    BeginAnswered();
    BlockAcked(0);

    ForwardTime(FirmwareTransfer::blockTimeout);
    EXPECT_EQ(services::fwuQueryProgressId, LastUpgradeFrame().messageType);

    ProgressAnswered(services::FwuState::receiving, 1, 4);
    EXPECT_EQ(services::fwuDataBlockId, LastUpgradeFrame().messageType);
    EXPECT_EQ(0, LastUpgradeFrame().payload[0]);
    EXPECT_EQ(1, LastUpgradeFrame().payload[1]);
}

TEST_F(TestFirmwareTransfer, three_tries_in_a_row_without_an_answer_give_the_download_up)
{
    ASSERT_TRUE(transfer.StartDownload(1, Pack(20)));
    BeginAnswered();

    ForwardTime(FirmwareTransfer::blockTimeout);
    ForwardTime(FirmwareTransfer::blockTimeout);
    EXPECT_CALL(observer, OnDownloadFinished(1, DownloadOutcome::noAnswer, FwuError::ok));
    ForwardTime(FirmwareTransfer::blockTimeout);
    EXPECT_EQ(services::fwuAbortId, LastUpgradeFrame().messageType);
}

TEST_F(TestFirmwareTransfer, an_acknowledged_block_starts_the_count_of_tries_again)
{
    ASSERT_TRUE(transfer.StartDownload(1, Pack(20)));
    BeginAnswered();
    ForwardTime(FirmwareTransfer::blockTimeout);
    ForwardTime(FirmwareTransfer::blockTimeout);
    ProgressAnswered(services::FwuState::receiving, 0, 4);
    BlockAcked(0);

    ForwardTime(FirmwareTransfer::blockTimeout);
    ForwardTime(FirmwareTransfer::blockTimeout);
    EXPECT_TRUE(transfer.IsActive());
}

TEST_F(TestFirmwareTransfer, an_acknowledgement_of_another_block_is_ignored)
{
    ASSERT_TRUE(transfer.StartDownload(1, Pack(20)));
    BeginAnswered();
    BlockAcked(0);
    const auto before = SentUpgradeFrames().size();

    BlockAcked(0);

    EXPECT_EQ(before, SentUpgradeFrames().size());
    EXPECT_EQ(6u, progresses.back().bytesSent);
}

TEST_F(TestFirmwareTransfer, a_block_refused_ends_the_download_with_the_refusal)
{
    ASSERT_TRUE(transfer.StartDownload(1, Pack(20)));
    BeginAnswered();

    EXPECT_CALL(observer, OnDownloadFinished(1, DownloadOutcome::refused, FwuError::writeError));
    BlockAcked(0, FwuError::writeError);
}

TEST_F(TestFirmwareTransfer, a_crc_mismatch_on_verify_ends_the_download_refused)
{
    DownloadUntilVerify(Pack(20));

    EXPECT_CALL(observer, OnDownloadFinished(1, DownloadOutcome::refused, FwuError::crcMismatch));
    VerifyAnswered(FwuError::crcMismatch);
}

TEST_F(TestFirmwareTransfer, cancel_sends_abort_and_ends_cancelled_once_acknowledged)
{
    ASSERT_TRUE(transfer.StartDownload(1, Pack(20)));
    BeginAnswered();

    transfer.Cancel();
    EXPECT_EQ(services::fwuAbortId, LastUpgradeFrame().messageType);
    EXPECT_EQ(TransferPhase::cancelling, transfer.Phase());

    EXPECT_CALL(observer, OnDownloadFinished(1, DownloadOutcome::cancelled, FwuError::ok));
    transfer.OnCommandAck(1, services::firmwareUpgradeCategoryId, services::fwuAbortId, services::CanAckStatus::success);
}

TEST_F(TestFirmwareTransfer, a_cancel_without_an_acknowledgement_still_ends_cancelled)
{
    ASSERT_TRUE(transfer.StartDownload(1, Pack(20)));
    transfer.Cancel();

    EXPECT_CALL(observer, OnDownloadFinished(1, DownloadOutcome::cancelled, FwuError::ok));
    ForwardTime(FirmwareTransfer::answerTimeout);
}

TEST(FirmwareTransferTimeLeft, the_time_left_follows_the_rate_so_far)
{
    EXPECT_FALSE(TimeLeft(0, 100, std::chrono::seconds(1)).has_value());
    EXPECT_EQ(std::chrono::seconds(3), TimeLeft(25, 100, std::chrono::seconds(1)));
    EXPECT_EQ(std::chrono::seconds(0), TimeLeft(100, 100, std::chrono::seconds(4)));
}
