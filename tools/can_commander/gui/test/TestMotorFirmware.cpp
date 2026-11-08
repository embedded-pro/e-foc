#include "application/bootloader_image/KeySet.hpp"
#include "application/bootloader_image/Pack.hpp"
#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeDefinitions.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "generated/Boards.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "tools/can_commander/gui/FirmwareTab.hpp"
#include "tools/can_commander/gui/MotorFirmware.hpp"
#include "tools/can_commander/gui/MotorPanel.hpp"
#include "tools/can_commander/logic/FirmwarePack.hpp"
#include "tools/can_commander/logic/test/CanBusAdapterMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <algorithm>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
    using namespace tool;
    using testing::_;
    using testing::AnyNumber;
    using testing::Invoke;
    using testing::StrictMock;

    constexpr const char* downloadButton = "&Download to Controller…";
    constexpr const char* applyButton = "&Apply Firmware…";
    constexpr const char* tryAgainButton = "&Try Again";

    struct Sent
    {
        uint16_t node;
        uint8_t category;
        uint8_t messageType;
    };

    bool Random(std::span<uint8_t> bytes)
    {
        static std::mt19937 engine{ 42 };
        std::ranges::generate(bytes, []()
            {
                return static_cast<uint8_t>(engine());
            });
        return true;
    }

    std::vector<uint8_t> PackFor(const e_foc::boards::Board& board)
    {
        const auto keys = application::GenerateKeySet(Random);
        const std::vector<uint8_t> image(600, 0x5a);
        const auto built = application::BuildPack(application::PackDescription{ board.identification, "1.7.0", "sync_foc_sensored", 0x8000, image }, *keys, Random);
        return std::get<std::vector<uint8_t>>(built);
    }

    class TestMotorFirmware
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        struct Expectations
        {
            explicit Expectations(TestMotorFirmware& test)
            {
                EXPECT_CALL(test.adapter, ReceiveData(_)).Times(2).WillRepeatedly([&test](const auto& callback)
                    {
                        test.receive = callback;
                    });
                EXPECT_CALL(test.adapter, SendData(_, _, _)).Times(AnyNumber()).WillRepeatedly(Invoke([&test](hal::Can::Id id, const hal::Can::Message&, const infra::Function<void(bool)>& done)
                    {
                        const auto raw = id.Get29BitId();
                        test.sent.push_back({ static_cast<uint16_t>(services::ExtractCanNodeId(raw)), services::ExtractCanCategory(raw), services::ExtractCanMessageType(raw) });
                        done(true);
                    }));
            }
        };

        void Receive(uint8_t category, uint8_t messageType, uint16_t node, std::initializer_list<uint8_t> bytes)
        {
            hal::Can::Message message;
            for (auto byte : bytes)
                message.push_back(byte);
            receive(hal::Can::Id::Create29BitId(services::MakeCanId(services::CanPriority::response, category, messageType, node)), message);
        }

        void BringCompatible(uint16_t node = 1)
        {
            Receive(services::canSystemCategoryId, services::canHeartbeatMessageTypeId, node, {});
            Receive(can::focMotorCategoryId, can::focContractVersionResponseId, node, { can::focContractVersionMajor, can::focContractVersionMinor });
        }

        void StoppedOn(const e_foc::boards::Board& board, services::FirmwareVersion staged = {})
        {
            BringCompatible();
            firmware.SetMotorState(FocMotorState::idle);
            firmware.OnFirmwareIdentity(FirmwareIdentity{ { 1, 6, 0 }, board.kind, staged });
        }

        QPushButton& Button(const QString& text)
        {
            for (auto* button : tab.findChildren<QPushButton*>())
                if (button->text() == text)
                    return *button;
            throw std::runtime_error{ "No button " + text.toStdString() };
        }

        static void AnswerNextQuestion(QMessageBox::StandardButton answer)
        {
            QTimer::singleShot(0, [answer]()
                {
                    auto* question = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                    ASSERT_NE(nullptr, question) << "No question was asked";
                    question->button(answer)->click();
                });
        }

        std::size_t UpgradeFrames(uint8_t messageType) const
        {
            return static_cast<std::size_t>(std::ranges::count_if(sent, [messageType](const Sent& frame)
                {
                    return frame.category == services::firmwareUpgradeCategoryId && frame.messageType == messageType;
                }));
        }

        StrictMock<CanBusAdapterMock> adapter;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receive;
        std::vector<Sent> sent;
        Expectations expectations{ *this };
        CanCommandClient client{ adapter };
        FirmwareTransfer transfer{ client };
        FirmwareTab tab;
        std::vector<QString> reports;
        QString unavailable;
        MotorFirmware firmware{ client, transfer, 1, tab, [this](IssueSeverity, const QString& message)
            {
                reports.push_back(message);
            },
            [this]()
            {
                return unavailable;
            },
            &tab };
        const std::vector<uint8_t> pack{ PackFor(e_foc::boards::ekTm4c1294xl) };
    };
}

TEST_F(TestMotorFirmware, nothing_is_offered_until_the_motor_reports_its_firmware)
{
    BringCompatible();
    firmware.SetMotorState(FocMotorState::idle);

    EXPECT_FALSE(Button(downloadButton).isEnabled());
    EXPECT_FALSE(Button(applyButton).isEnabled());
    EXPECT_TRUE(Button(downloadButton).toolTip().contains("Reading the firmware"));
}

TEST_F(TestMotorFirmware, a_checked_pack_for_the_board_enables_the_download_on_a_stopped_motor)
{
    StoppedOn(e_foc::boards::ekTm4c1294xl);

    EXPECT_TRUE(firmware.OfferPackFile(pack, "release 1.7.0"));

    EXPECT_TRUE(Button(downloadButton).isEnabled());
    EXPECT_FALSE(Button(applyButton).isEnabled());
    EXPECT_TRUE(Button(applyButton).toolTip().contains("No pack is staged"));
}

TEST_F(TestMotorFirmware, a_pack_for_another_board_is_not_offered)
{
    StoppedOn(e_foc::boards::emulated);

    EXPECT_FALSE(firmware.OfferPackFile(pack, "pack.upgrade"));

    EXPECT_FALSE(Button(downloadButton).isEnabled());
    EXPECT_TRUE(reports.back().contains("EK-TM4C1294XL (TM4C1294NCPDT)"));
}

TEST_F(TestMotorFirmware, download_and_apply_are_disabled_while_the_motor_runs)
{
    StoppedOn(e_foc::boards::ekTm4c1294xl, { 1, 7, 0 });
    firmware.OfferPackFile(pack, "release 1.7.0");

    firmware.SetMotorState(FocMotorState::running);

    EXPECT_FALSE(Button(downloadButton).isEnabled());
    EXPECT_FALSE(Button(applyButton).isEnabled());
    EXPECT_TRUE(Button(downloadButton).toolTip().contains("Stop the motor first"));
    EXPECT_TRUE(Button(applyButton).toolTip().contains("Stop the motor first"));
}

TEST_F(TestMotorFirmware, download_and_apply_are_disabled_while_the_motor_has_a_command_pending)
{
    StoppedOn(e_foc::boards::ekTm4c1294xl, { 1, 7, 0 });
    firmware.OfferPackFile(pack, "release 1.7.0");

    unavailable = "Waiting for the response to Start Motor…";
    firmware.Refresh();

    EXPECT_FALSE(Button(downloadButton).isEnabled());
    EXPECT_FALSE(Button(applyButton).isEnabled());
    EXPECT_EQ(unavailable, Button(downloadButton).toolTip());
}

TEST_F(TestMotorFirmware, download_is_disabled_while_another_motor_downloads)
{
    StoppedOn(e_foc::boards::ekTm4c1294xl);
    firmware.OfferPackFile(pack, "release 1.7.0");

    ASSERT_TRUE(transfer.StartDownload(2, *InspectPack(pack, *BoardOfKind(e_foc::boards::ekTm4c1294xl.kind)).pack));
    firmware.Refresh();

    EXPECT_FALSE(Button(downloadButton).isEnabled());
    EXPECT_TRUE(Button(downloadButton).toolTip().contains("Motor 2 is busy with its firmware"));
}

TEST_F(TestMotorFirmware, a_staged_pack_enables_the_apply_on_a_stopped_motor)
{
    StoppedOn(e_foc::boards::ekTm4c1294xl, { 1, 7, 0 });

    EXPECT_TRUE(Button(applyButton).isEnabled());
}

TEST_F(TestMotorFirmware, the_download_sends_begin_only_after_the_user_confirms)
{
    StoppedOn(e_foc::boards::ekTm4c1294xl);
    firmware.OfferPackFile(pack, "release 1.7.0");

    AnswerNextQuestion(QMessageBox::Cancel);
    Button(downloadButton).click();
    EXPECT_EQ(0, UpgradeFrames(services::fwuBeginUpgradeId));

    AnswerNextQuestion(QMessageBox::Yes);
    Button(downloadButton).click();
    EXPECT_EQ(1, UpgradeFrames(services::fwuBeginUpgradeId));
}

TEST_F(TestMotorFirmware, the_apply_sends_activate_only_after_the_user_confirms)
{
    StoppedOn(e_foc::boards::ekTm4c1294xl, { 1, 7, 0 });

    AnswerNextQuestion(QMessageBox::Cancel);
    Button(applyButton).click();
    EXPECT_EQ(0, UpgradeFrames(services::fwuActivateId));

    AnswerNextQuestion(QMessageBox::Yes);
    Button(applyButton).click();
    EXPECT_EQ(1, UpgradeFrames(services::fwuActivateId));
}

TEST_F(TestMotorFirmware, try_again_after_a_failed_download_asks_the_confirmation_again)
{
    StoppedOn(e_foc::boards::ekTm4c1294xl);
    firmware.OfferPackFile(pack, "release 1.7.0");
    firmware.OnDownloadFinished(DownloadOutcome::refused, services::FwuError::busy);
    firmware.Refresh();
    ASSERT_TRUE(Button(tryAgainButton).isVisibleTo(&tab));

    AnswerNextQuestion(QMessageBox::Cancel);
    Button(tryAgainButton).click();
    EXPECT_EQ(0, UpgradeFrames(services::fwuBeginUpgradeId));

    AnswerNextQuestion(QMessageBox::Yes);
    Button(tryAgainButton).click();
    EXPECT_EQ(1, UpgradeFrames(services::fwuBeginUpgradeId));
}

TEST_F(TestMotorFirmware, a_failed_apply_offers_no_try_again)
{
    StoppedOn(e_foc::boards::ekTm4c1294xl, { 1, 7, 0 });
    firmware.OfferPackFile(pack, "release 1.7.0");

    firmware.OnApplyFinished(ApplyOutcome::oldVersionKept, services::FwuError::ok, { 1, 6, 0 });
    firmware.Refresh();

    EXPECT_FALSE(Button(tryAgainButton).isVisibleTo(&tab));
}

TEST_F(TestMotorFirmware, the_emergency_stop_stays_available_to_a_motor_during_its_download)
{
    MotorPanel panel{ client, transfer, 1, [](IssueSeverity, const QString&) {} };
    panel.SetBusConnected(true);
    BringCompatible();
    panel.OnContractVersion(can::focContractVersionMajor, can::focContractVersionMinor, true);
    ASSERT_TRUE(transfer.StartDownload(1, *InspectPack(pack, *BoardOfKind(e_foc::boards::ekTm4c1294xl.kind)).pack));

    EXPECT_TRUE(panel.AcceptsEmergencyStop());
    EXPECT_TRUE(panel.SendEmergencyStop());
    EXPECT_EQ(1, std::ranges::count_if(sent, [](const Sent& frame)
                     {
                         return frame.category == can::focMotorCategoryId && frame.messageType == can::focEmergencyStopId;
                     }));
}
