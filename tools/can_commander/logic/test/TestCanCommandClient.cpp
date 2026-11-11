#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeDefinitions.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "e-foc-protocol/contract/FocMotorWireContract.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "tools/can_commander/logic/test/CanBusAdapterMock.hpp"
#include "tools/can_commander/logic/test/CanCommandClientObserverMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <bit>
#include <limits>
#include <set>
#include <vector>

namespace
{
    using namespace tool;
    using testing::_;
    using testing::Invoke;
    using testing::StrictMock;

    constexpr uint8_t undecodedResponseId{ 0x80 };

    class TestCanCommandClient
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        struct FixtureInit
        {
            FixtureInit(StrictMock<CanBusAdapterMock>& adapter,
                infra::Function<void(hal::Can::Id, const hal::Can::Message&)>& receiveCallback, bool sendSucceeds)
            {
                // Registered by the client's constructor and deregistered by its destructor
                EXPECT_CALL(adapter, ReceiveData(_)).Times(2).WillRepeatedly([&receiveCallback](const auto& callback)
                    {
                        receiveCallback = callback;
                    });
                EXPECT_CALL(adapter, SendData(_, _, _))
                    .Times(testing::AnyNumber())
                    .WillRepeatedly(Invoke([sendSucceeds](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                        {
                            cb(sendSucceeds);
                        }));
            }
        };

        explicit TestCanCommandClient(bool sendSucceeds = true)
            : fixtureInit{ adapter, receiveCallback, sendSucceeds }
        {}

        void SetUp() override
        {
            EXPECT_CALL(observer, OnBusyChanged(_, _)).Times(testing::AnyNumber());
        }

        static uint32_t FocId(services::CanPriority priority, uint8_t messageType, uint16_t node)
        {
            return services::MakeCanId(priority, can::focMotorCategoryId, messageType, node);
        }

        static hal::Can::Message Message(std::initializer_list<uint8_t> bytes)
        {
            hal::Can::Message message;
            for (auto byte : bytes)
                message.push_back(byte);
            return message;
        }

        void Receive(uint32_t rawId, const hal::Can::Message& data)
        {
            receiveCallback(hal::Can::Id::Create29BitId(rawId), data);
        }

        void LogReceived(uint32_t rawId, const hal::Can::Message& data)
        {
            services::CanFrame frame;
            for (auto byte : data)
                frame.push_back(byte);

            adapter.NotifyObservers([rawId, &frame](auto& observer)
                {
                    observer.OnFrameLog(false, rawId, frame);
                });
        }

        void Heartbeat(uint16_t node)
        {
            Receive(services::MakeCanId(services::CanPriority::heartbeat, services::canSystemCategoryId, services::canHeartbeatMessageTypeId, node), {});
        }

        void BringOnline(uint16_t node)
        {
            EXPECT_CALL(observer, OnNodeOnlineChanged(node, true));
            Heartbeat(node);
        }

        void DeliverAck(uint8_t commandType, uint16_t node = 1, services::CanAckStatus status = services::CanAckStatus::success)
        {
            const auto rawId = services::MakeCanId(services::CanPriority::response, services::canSystemCategoryId, services::canCommandAckMessageTypeId, node);
            LogReceived(rawId, Message({ can::focMotorCategoryId, commandType, static_cast<uint8_t>(status), 0 }));
        }

        void DeliverParameter(uint16_t node, can::FocParameter parameter, float value)
        {
            const auto bits = std::bit_cast<uint32_t>(value);
            Receive(FocId(services::CanPriority::response, can::focParameterResponseId, node),
                Message({ static_cast<uint8_t>(parameter), 0, static_cast<uint8_t>(bits >> 24), static_cast<uint8_t>(bits >> 16), static_cast<uint8_t>(bits >> 8), static_cast<uint8_t>(bits) }));
        }

        void DeliverRunningIdentity(uint16_t node, const FirmwareIdentity& identity)
        {
            DeliverParameter(node, can::FocParameter::firmwareVersionMajor, identity.running.major);
            DeliverParameter(node, can::FocParameter::firmwareVersionMinor, identity.running.minor);
            DeliverParameter(node, can::FocParameter::firmwareVersionPatch, identity.running.patch);
            DeliverParameter(node, can::FocParameter::boardKind, identity.boardKind);
        }

        void DeliverIdentity(uint16_t node, const FirmwareIdentity& identity)
        {
            DeliverRunningIdentity(node, identity);
            DeliverParameter(node, can::FocParameter::stagedVersionMajor, identity.staged.major);
            DeliverParameter(node, can::FocParameter::stagedVersionMinor, identity.staged.minor);
            DeliverParameter(node, can::FocParameter::stagedVersionPatch, identity.staged.patch);
        }

        void DeliverContract(uint8_t major, uint16_t node, uint8_t minor = can::focContractVersionMinor)
        {
            Receive(FocId(services::CanPriority::response, can::focContractVersionResponseId, node), Message({ major, minor }));
        }

        static hal::Can::Message AfterSequence(const hal::Can::Message& payload)
        {
            hal::Can::Message fields;
            for (std::size_t i = 1; i < payload.size(); ++i)
                fields.push_back(payload[i]);
            return fields;
        }

        struct SentFrame
        {
            uint16_t node;
            uint8_t messageType;
            hal::Can::Message payload;
        };

        void RecordSends()
        {
            EXPECT_CALL(adapter, SendData(_, _, _))
                .Times(testing::AnyNumber())
                .WillRepeatedly(Invoke([this](hal::Can::Id id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                    {
                        if (services::ExtractCanCategory(id.Get29BitId()) == can::focMotorCategoryId)
                            sent.push_back({ static_cast<uint16_t>(services::ExtractCanNodeId(id.Get29BitId())), services::ExtractCanMessageType(id.Get29BitId()), msg });
                        cb(true);
                    }));
        }

        std::vector<uint8_t> SentTypes() const
        {
            std::vector<uint8_t> types;
            for (const auto& frame : sent)
                types.push_back(frame.messageType);
            return types;
        }

        static std::size_t WritableParameterCount()
        {
            return static_cast<std::size_t>(std::ranges::count_if(can::focParameters, [](const can::ParameterDescriptor& descriptor)
                {
                    return !descriptor.readOnly;
                }));
        }

        static std::size_t ReadOnlyParameterCount()
        {
            return can::focParameters.size() - WritableParameterCount();
        }

        static constexpr std::size_t readWithTheStatus{ 3 };

        bool Queried(can::FocParameter parameter) const
        {
            return std::ranges::any_of(sent, [parameter](const SentFrame& frame)
                {
                    return frame.messageType == can::focQueryParameterId && frame.payload.size() >= 2 &&
                           frame.payload[1] == static_cast<uint8_t>(parameter);
                });
        }

        std::size_t Count(uint8_t messageType) const
        {
            return static_cast<std::size_t>(std::count_if(sent.begin(), sent.end(), [messageType](const SentFrame& frame)
                {
                    return frame.messageType == messageType;
                }));
        }

        void AnswerLastRead(uint16_t node = 1)
        {
            DeliverAck(sent.back().messageType, node);
        }

        void AnswerAllReads(uint16_t node = 1)
        {
            for (auto before = std::size_t{ 0 }; before != sent.size();)
            {
                before = sent.size();
                AnswerLastRead(node);
            }
        }

        void BringCompatible(uint16_t node)
        {
            BringOnline(node);
            EXPECT_CALL(observer, OnContractVersion(node, can::focContractVersionMajor, can::focContractVersionMinor, true));
            DeliverContract(can::focContractVersionMajor, node);
        }

        void DeliverTelemetryStatus(uint16_t node, FocMotorState state, FocFaultCode fault)
        {
            Receive(FocId(services::CanPriority::telemetry, can::focTelemetryStatusResponseId, node), Message({ static_cast<uint8_t>(state), static_cast<uint8_t>(fault) }));
        }

        void SetBusConnected(bool connected)
        {
            adapter.NotifyObservers([connected](auto& observer)
                {
                    observer.OnConnectionChanged(connected);
                });
        }

        hal::Can::Id SendStartAndCaptureId(uint16_t node)
        {
            hal::Can::Id sentId{ hal::Can::Id::Create11BitId(0) };
            EXPECT_CALL(adapter, SendData(_, _, _))
                .WillOnce(Invoke([&sentId](hal::Can::Id id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                    {
                        sentId = id;
                        cb(true);
                    }));
            client.Commands().SendStartMotor(node);
            return sentId;
        }

        void CaptureNextPayload()
        {
            EXPECT_CALL(adapter, SendData(_, _, _))
                .WillOnce(Invoke([this](hal::Can::Id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                    {
                        capturedPayload = msg;
                        cb(true);
                    }));
        }

        StrictMock<CanBusAdapterMock> adapter;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receiveCallback;
        FixtureInit fixtureInit;
        CanCommandClient client{ adapter };
        StrictMock<CanCommandClientObserverMock> observer{ client };
        hal::Can::Message capturedPayload;
        std::vector<SentFrame> sent;
    };

    class TestCanCommandClientAdapterFails
        : public TestCanCommandClient
    {
    public:
        TestCanCommandClientAdapterFails()
            : TestCanCommandClient(false)
        {}
    };
}

TEST_F(TestCanCommandClient, initially_no_node_is_busy_or_online)
{
    EXPECT_FALSE(client.IsBusy(1));
    EXPECT_FALSE(client.IsOnline(1));
    EXPECT_EQ(can::ContractCompatibility::unknown, client.Compatibility(1));
}

TEST_F(TestCanCommandClient, commands_are_addressed_to_the_given_node)
{
    EXPECT_EQ(services::ExtractCanNodeId(SendStartAndCaptureId(1).Get29BitId()), 1u);
    EXPECT_EQ(services::ExtractCanNodeId(SendStartAndCaptureId(5).Get29BitId()), 5u);
}

TEST_F(TestCanCommandClient, each_node_tracks_its_own_pending_command)
{
    client.Commands().SendStartMotor(1);

    EXPECT_TRUE(client.IsBusy(1));
    EXPECT_FALSE(client.IsBusy(2));
}

TEST_F(TestCanCommandClient, a_heartbeat_reports_its_node_online_once)
{
    BringOnline(1);
    Heartbeat(1);

    EXPECT_TRUE(client.IsOnline(1));
}

TEST_F(TestCanCommandClient, heartbeats_from_several_nodes_report_each_of_them)
{
    BringOnline(1);
    BringOnline(7);

    EXPECT_TRUE(client.IsOnline(1));
    EXPECT_TRUE(client.IsOnline(7));
    EXPECT_EQ((std::set<uint16_t>{ 1, 7 }), client.OnlineNodes());
}

TEST_F(TestCanCommandClient, a_received_frame_reports_its_sender_online_before_it_is_decoded)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(3, true));
    EXPECT_CALL(observer, OnFrameLog(false, _, _));

    LogReceived(services::MakeCanId(services::CanPriority::response, can::focMotorCategoryId, undecodedResponseId, 3), Message({ 0 }));
}

TEST_F(TestCanCommandClient, a_silent_node_goes_offline_after_the_server_timeout)
{
    BringOnline(1);

    EXPECT_CALL(observer, OnNodeOnlineChanged(1, false));
    ForwardTime(std::chrono::seconds(4));

    EXPECT_FALSE(client.IsOnline(1));
}

TEST_F(TestCanCommandClient, going_offline_ends_the_nodes_pending_command)
{
    BringOnline(1);
    ForwardTime(std::chrono::milliseconds(2500));
    client.Commands().SendStartMotor(1);

    EXPECT_CALL(observer, OnNodeOnlineChanged(1, false));
    ForwardTime(std::chrono::seconds(1));
    EXPECT_FALSE(client.IsBusy(1));

    ForwardTime(std::chrono::seconds(2));
}

TEST_F(TestCanCommandClient, closing_the_bus_reports_nodes_offline_and_ends_pending_commands_at_once)
{
    BringOnline(1);
    client.Commands().SendStartMotor(1);

    {
        testing::InSequence sequence;
        EXPECT_CALL(observer, OnNodeOnlineChanged(1, false));
        EXPECT_CALL(observer, OnConnectionChanged(false));
    }
    SetBusConnected(false);

    EXPECT_FALSE(client.IsBusy(1));
    EXPECT_FALSE(client.IsOnline(1));
    ForwardTime(std::chrono::seconds(5));
}

TEST_F(TestCanCommandClient, a_node_heard_again_after_reconnecting_is_reported_online_again)
{
    const auto statusId = FocId(services::CanPriority::telemetry, undecodedResponseId, 1);
    EXPECT_CALL(observer, OnFrameLog(false, statusId, _)).Times(2);
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true)).Times(2);
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, false));
    EXPECT_CALL(observer, OnConnectionChanged(false));
    EXPECT_CALL(observer, OnConnectionChanged(true));

    LogReceived(statusId, Message({ 0 }));
    SetBusConnected(false);
    SetBusConnected(true);
    LogReceived(statusId, Message({ 0 }));

    EXPECT_TRUE(client.IsOnline(1));
}

TEST_F(TestCanCommandClient, transmitted_frames_are_relayed_to_the_log)
{
    services::CanFrame data;
    data.push_back(0x42);

    EXPECT_CALL(observer, OnFrameLog(true, 0x12345u, _));

    adapter.NotifyObservers([&data](auto& obs)
        {
            obs.OnFrameLog(true, 0x12345u, data);
        });
}

TEST_F(TestCanCommandClient, connection_changed_relayed)
{
    EXPECT_CALL(observer, OnConnectionChanged(true));

    SetBusConnected(true);
}

TEST_F(TestCanCommandClient, adapter_error_relayed)
{
    EXPECT_CALL(observer, OnAdapterError(_));

    adapter.NotifyObservers([](auto& obs)
        {
            obs.OnError("test error");
        });
}

TEST_F(TestCanCommandClient, every_command_stays_pending_until_answered)
{
    EXPECT_TRUE(client.Commands().SendStartMotor(1));
    EXPECT_TRUE(client.Commands().SendStopMotor(2));
    EXPECT_TRUE(client.Commands().SendEmergencyStop(3));
    EXPECT_TRUE(client.Commands().SendSetControlMode(4, can::FocMotorMode::speed));
    EXPECT_TRUE(client.Commands().SendSetTorqueSetpoint(5, 5.0f));
    EXPECT_TRUE(client.Commands().SendSetSpeedSetpoint(6, 100.0f));
    EXPECT_TRUE(client.Commands().SendSetPositionSetpoint(7, 1.0f));
    EXPECT_TRUE(client.Commands().SendSetTorqueSetpointNewtonMeter(8, 0.1f));
    EXPECT_TRUE(client.Commands().SendClearFault(9));
    EXPECT_TRUE(client.Commands().SendAlign(10));
    EXPECT_TRUE(client.Commands().SendIdentifyElectrical(11));
    EXPECT_TRUE(client.Commands().SendSelectAlgorithm(12, can::FocControlLoop::speed, 1));
    EXPECT_TRUE(client.Commands().SendSetParameter(13, can::FocParameter::inertia, 1.0e-5f));

    for (uint16_t node = 1; node <= 13; ++node)
        EXPECT_TRUE(client.IsBusy(node)) << node;
}

TEST_F(TestCanCommandClient, overrange_setpoints_are_still_sent_and_pending)
{
    client.Commands().SendSetSpeedSetpoint(1, 100000.0f);
    client.Commands().SendSetSpeedSetpoint(2, -100000.0f);
    client.Commands().SendSetPositionSetpoint(3, 1000.0f);
    client.Commands().SendSetPositionSetpoint(4, -1000.0f);

    for (uint16_t node = 1; node <= 4; ++node)
        EXPECT_TRUE(client.IsBusy(node)) << node;
}

TEST_F(TestCanCommandClient, request_data_does_not_make_the_node_busy)
{
    EXPECT_TRUE(client.Data().RequestData(1));
    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, an_acknowledgement_ends_the_pending_command)
{
    testing::InSequence sequence;
    EXPECT_CALL(observer, OnBusyChanged(1, true));
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnBusyChanged(1, false));
    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focStartId, services::CanAckStatus::success));

    client.Commands().SendStartMotor(1);
    DeliverAck(can::focStartId);

    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, an_acknowledgement_from_another_node_leaves_the_command_pending)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(2, true));
    EXPECT_CALL(observer, OnCommandAck(2, can::focMotorCategoryId, can::focStartId, services::CanAckStatus::success));

    client.Commands().SendStartMotor(1);
    DeliverAck(can::focStartId, 2);

    EXPECT_TRUE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, a_late_acknowledgement_of_an_older_command_keeps_the_newer_one_pending)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focStartId, _));
    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focEmergencyStopId, _));

    client.Commands().SendStartMotor(1);
    client.Commands().SendEmergencyStop(1);

    DeliverAck(can::focStartId);
    EXPECT_TRUE(client.IsBusy(1));

    DeliverAck(can::focEmergencyStopId);
    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, a_rejection_reports_the_reason_and_ends_the_pending_command)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnCommandRejected(1, can::focSetTorqueSetpointId, can::FocMotorCategoryError::modeMismatch));

    client.Commands().SendSetTorqueSetpoint(1, 2.0f);
    Receive(FocId(services::CanPriority::response, services::canCategoryErrorResponseMessageTypeId, 1),
        Message({ can::focSetTorqueSetpointId, static_cast<uint8_t>(can::FocMotorCategoryError::modeMismatch) }));

    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, a_pending_command_times_out_after_the_command_timeout)
{
    client.Commands().SendStopMotor(1);

    ForwardTime(CommandTracker::commandTimeout - std::chrono::milliseconds(1));
    EXPECT_TRUE(client.IsBusy(1));

    EXPECT_CALL(observer, OnCommandTimeout(1, can::focStopId));
    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, a_newer_command_restarts_the_timeout)
{
    client.Commands().SendStartMotor(1);
    ForwardTime(std::chrono::milliseconds(1500));
    client.Commands().SendEmergencyStop(1);

    ForwardTime(std::chrono::milliseconds(1500));
    EXPECT_TRUE(client.IsBusy(1));

    EXPECT_CALL(observer, OnCommandTimeout(1, can::focEmergencyStopId));
    ForwardTime(std::chrono::milliseconds(500));
}

TEST_F(TestCanCommandClient, an_incompatible_node_refuses_commands_without_touching_its_pending_command)
{
    BringOnline(1);
    client.Commands().SendStartMotor(1);

    EXPECT_CALL(observer, OnContractVersion(1, static_cast<uint8_t>(can::focContractVersionMajor + 1), can::focContractVersionMinor, false));
    DeliverContract(static_cast<uint8_t>(can::focContractVersionMajor + 1), 1);

    EXPECT_EQ(can::ContractCompatibility::incompatible, client.Compatibility(1));
    EXPECT_FALSE(client.Commands().SendEmergencyStop(1));
    EXPECT_TRUE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, a_node_on_another_minor_version_is_incompatible)
{
    BringOnline(1);
    const auto otherMinor = static_cast<uint8_t>(can::focContractVersionMinor + 1);

    EXPECT_CALL(observer, OnContractVersion(1, can::focContractVersionMajor, otherMinor, false));
    DeliverContract(can::focContractVersionMajor, 1, otherMinor);

    EXPECT_EQ(can::ContractCompatibility::incompatible, client.Compatibility(1));
    EXPECT_FALSE(client.Commands().SendStartMotor(1));
}

TEST_F(TestCanCommandClient, an_incompatible_node_does_not_block_the_others)
{
    BringOnline(1);
    BringOnline(2);
    EXPECT_CALL(observer, OnContractVersion(1, _, _, false));
    EXPECT_CALL(observer, OnContractVersion(2, can::focContractVersionMajor, can::focContractVersionMinor, true));

    DeliverContract(static_cast<uint8_t>(can::focContractVersionMajor + 1), 1);
    DeliverContract(can::focContractVersionMajor, 2);

    EXPECT_FALSE(client.Commands().SendEmergencyStop(1));
    EXPECT_TRUE(client.Commands().SendEmergencyStop(2));
    EXPECT_EQ(can::ContractCompatibility::compatible, client.Compatibility(2));
}

TEST_F(TestCanCommandClient, telemetry_status_notifies_the_motor_status)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnMotorStatusReceived(1, FocMotorState::running, FocFaultCode::none));

    DeliverTelemetryStatus(1, FocMotorState::running, FocFaultCode::none);
}

TEST_F(TestCanCommandClient, telemetry_motion_reports_speed_and_position_at_full_resolution)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnSpeedPositionReceived(1, 0.005f, 0.00015f));

    Receive(FocId(services::CanPriority::telemetry, can::focTelemetryMotionId, 1), Message({ 0x3B, 0xA3, 0xD7, 0x0A, 0x39, 0x1D, 0x49, 0x52 }));
}

TEST_F(TestCanCommandClient, telemetry_is_attributed_to_the_node_that_sent_it)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(2, true));
    EXPECT_CALL(observer, OnNodeOnlineChanged(3, true));
    EXPECT_CALL(observer, OnMotorStatusReceived(2, FocMotorState::running, FocFaultCode::none));
    EXPECT_CALL(observer, OnMotorStatusReceived(3, FocMotorState::idle, FocFaultCode::none));

    DeliverTelemetryStatus(2, FocMotorState::running, FocFaultCode::none);
    DeliverTelemetryStatus(3, FocMotorState::idle, FocFaultCode::none);
}

TEST_F(TestCanCommandClient, telemetry_status_ready_is_passed_on_as_ready)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnMotorStatusReceived(1, FocMotorState::ready, FocFaultCode::none));

    Receive(FocId(services::CanPriority::telemetry, can::focTelemetryStatusResponseId, 1), Message({ 5, 0 }));
}

TEST_F(TestCanCommandClient, telemetry_status_with_a_hardware_fault_notifies_fault_event)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnMotorStatusReceived(1, FocMotorState::fault, FocFaultCode::hardwareFault));
    EXPECT_CALL(observer, OnFaultEventReceived(1, FocFaultCode::hardwareFault));

    Receive(FocId(services::CanPriority::telemetry, can::focTelemetryStatusResponseId, 1), Message({ 2, 8 }));
}

TEST_F(TestCanCommandClient, telemetry_status_with_fault_notifies_fault_event)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnMotorStatusReceived(1, FocMotorState::fault, FocFaultCode::overCurrent));
    EXPECT_CALL(observer, OnFaultEventReceived(1, FocFaultCode::overCurrent));

    DeliverTelemetryStatus(1, FocMotorState::fault, FocFaultCode::overCurrent);
}

TEST_F(TestCanCommandClient, telemetry_frames_stay_out_of_the_frame_log)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));

    LogReceived(FocId(services::CanPriority::telemetry, can::focTelemetryStatusResponseId, 1), Message({ 1, 0 }));
    LogReceived(FocId(services::CanPriority::telemetry, can::focTelemetryMotionId, 1), Message({ 0, 0, 0, 0, 0, 0, 0, 0 }));
}

TEST_F(TestCanCommandClient, a_current_stream_frame_notifies_the_sample_of_its_node)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(4, true));
    EXPECT_CALL(observer, OnCurrentStreamReceived(4, 0x2A, testing::FloatNear(0.512f, 0.0001f), testing::FloatNear(-1.0f, 0.0001f), testing::FloatNear(-3.1416f, 0.0001f)));

    Receive(FocId(services::CanPriority::telemetry, can::focTelemetryCurrentsId, 4), Message({ 0x2A, 0x02, 0x00, 0xFC, 0x18, 0x85, 0x48 }));
}

TEST_F(TestCanCommandClient, current_stream_frames_stay_out_of_the_frame_log)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(4, true));

    for (uint8_t sequence = 0; sequence != 3; ++sequence)
        LogReceived(FocId(services::CanPriority::telemetry, can::focTelemetryCurrentsId, 4), Message({ sequence, 0, 0, 0, 0, 0, 0 }));
}

TEST_F(TestCanCommandClient, an_undecoded_response_does_not_call_any_observer)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));

    Receive(FocId(services::CanPriority::response, undecodedResponseId, 1), Message({ static_cast<uint8_t>(can::FocMotorMode::speed) }));
}

TEST_F(TestCanCommandClient, select_control_mode_response_acknowledges_the_mode_and_ends_the_command)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnControlModeAcknowledged(1, can::FocMotorMode::speed));

    client.Commands().SendSetControlMode(1, can::FocMotorMode::speed);
    Receive(FocId(services::CanPriority::response, can::focSelectControlModeResponseId, 1), Message({ static_cast<uint8_t>(can::FocMotorMode::speed) }));

    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, the_setpoints_travel_as_big_endian_floats)
{
    RecordSends();

    EXPECT_TRUE(client.Commands().SendSetTorqueSetpoint(1, 0.0005f));
    EXPECT_TRUE(client.Commands().SendSetSpeedSetpoint(1, 3.25f));
    EXPECT_TRUE(client.Commands().SendSetPositionSetpoint(1, -1.5f));

    ASSERT_EQ(3u, sent.size());
    EXPECT_EQ(can::focSetTorqueSetpointId, sent[0].messageType);
    EXPECT_EQ(Message({ 0x3A, 0x03, 0x12, 0x6F }), AfterSequence(sent[0].payload));
    EXPECT_EQ(can::focSetSpeedSetpointId, sent[1].messageType);
    EXPECT_EQ(Message({ 0x40, 0x50, 0x00, 0x00 }), AfterSequence(sent[1].payload));
    EXPECT_EQ(can::focSetPositionSetpointId, sent[2].messageType);
    EXPECT_EQ(Message({ 0xBF, 0xC0, 0x00, 0x00 }), AfterSequence(sent[2].payload));
}

TEST_F(TestCanCommandClient, a_torque_in_newton_metres_goes_out_as_its_own_command)
{
    RecordSends();

    EXPECT_TRUE(client.Commands().SendSetTorqueSetpointNewtonMeter(2, 0.1f));

    ASSERT_EQ(1u, sent.size());
    EXPECT_EQ(2u, sent[0].node);
    EXPECT_EQ(can::focSetTorqueSetpointNewtonMeterId, sent[0].messageType);
    EXPECT_EQ(Message({ 0x3D, 0xCC, 0xCC, 0xCD }), AfterSequence(sent[0].payload));
}

TEST_F(TestCanCommandClient, a_setpoint_that_is_not_finite_is_not_sent)
{
    RecordSends();

    EXPECT_FALSE(client.Commands().SendSetSpeedSetpoint(1, std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(client.Commands().SendSetTorqueSetpointNewtonMeter(1, std::numeric_limits<float>::infinity()));

    EXPECT_TRUE(sent.empty());
    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, the_acknowledge_of_a_setpoint_ends_it)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    client.Commands().SendSetSpeedSetpoint(1, 3.25f);
    EXPECT_TRUE(client.IsBusy(1));

    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focSetSpeedSetpointId, services::CanAckStatus::success));
    DeliverAck(can::focSetSpeedSetpointId);

    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, the_refusal_of_a_newton_metre_torque_names_that_command)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    client.Commands().SendSetTorqueSetpointNewtonMeter(1, 0.1f);

    EXPECT_CALL(observer, OnCommandRejected(1, can::focSetTorqueSetpointNewtonMeterId, can::FocMotorCategoryError::modeMismatch));
    Receive(FocId(services::CanPriority::response, services::canCategoryErrorResponseMessageTypeId, 1),
        Message({ can::focSetTorqueSetpointNewtonMeterId, static_cast<uint8_t>(can::FocMotorCategoryError::modeMismatch) }));

    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClientAdapterFails, every_command_stays_pending_when_the_adapter_reports_failure)
{
    client.Commands().SendStartMotor(1);
    client.Commands().SendStopMotor(2);
    client.Commands().SendEmergencyStop(3);
    client.Commands().SendSetControlMode(4, can::FocMotorMode::speed);
    client.Commands().SendSetTorqueSetpoint(5, 5.0f);
    client.Commands().SendSetSpeedSetpoint(6, 10.0f);
    client.Commands().SendSetPositionSetpoint(7, 1.0f);

    for (uint16_t node = 1; node <= 7; ++node)
        EXPECT_TRUE(client.IsBusy(node)) << node;
}

TEST_F(TestCanCommandClientAdapterFails, timeout_after_failed_send_ends_the_pending_command)
{
    client.Commands().SendStopMotor(1);

    EXPECT_CALL(observer, OnCommandTimeout(1, can::focStopId));
    ForwardTime(CommandTracker::commandTimeout);

    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, a_compatible_node_is_read_one_request_at_a_time)
{
    BringOnline(1);
    RecordSends();
    EXPECT_CALL(observer, OnContractVersion(1, can::focContractVersionMajor, can::focContractVersionMinor, true));
    DeliverContract(can::focContractVersionMajor, 1);

    EXPECT_EQ((std::vector<uint8_t>{ can::focQueryAlgorithmsId }), SentTypes());
    EXPECT_FALSE(client.IsBusy(1));

    AnswerLastRead();

    ASSERT_EQ(2u, sent.size());
    EXPECT_EQ(can::focQueryParameterId, sent.back().messageType);
    EXPECT_EQ(static_cast<uint8_t>(can::FocParameter::currentBandwidth), sent.back().payload[1]);
}

TEST_F(TestCanCommandClient, a_compatible_node_is_read_for_every_parameter_once_its_fault_history_and_its_status)
{
    RecordSends();
    BringCompatible(1);

    AnswerAllReads();

    EXPECT_EQ(1u, Count(can::focQueryAlgorithmsId));
    EXPECT_EQ(can::focParameters.size(), Count(can::focQueryParameterId));
    EXPECT_EQ(can::focFaultHistoryDepth, Count(can::focQueryFaultHistoryId));
    EXPECT_EQ(1u, Count(can::focRequestTelemetryId));
    EXPECT_TRUE(Queried(can::FocParameter::telemetryRate));
}

TEST_F(TestCanCommandClient, an_incompatible_node_is_read_for_its_firmware_identity_only)
{
    BringOnline(1);
    RecordSends();
    EXPECT_CALL(observer, OnContractVersion(1, _, _, false));
    DeliverContract(static_cast<uint8_t>(can::focContractVersionMajor + 1), 1);

    ForwardTime(CanCommandClient::statusRefreshInterval);

    ASSERT_FALSE(sent.empty());
    EXPECT_TRUE(Queried(can::FocParameter::firmwareVersionMajor));
    EXPECT_EQ(sent.size(), Count(can::focQueryParameterId));
    for (const auto& frame : sent)
        EXPECT_TRUE(can::IsFirmwareIdentity(static_cast<can::FocParameter>(frame.payload[1]))) << static_cast<int>(frame.payload[1]);
}

TEST_F(TestCanCommandClient, an_unanswered_read_gives_way_to_the_next_after_the_read_timeout)
{
    BringOnline(1);
    RecordSends();
    EXPECT_CALL(observer, OnContractVersion(1, can::focContractVersionMajor, can::focContractVersionMinor, true));
    DeliverContract(can::focContractVersionMajor, 1);

    ForwardTime(ReadScheduler::readTimeout - std::chrono::milliseconds(1));
    EXPECT_EQ((std::vector<uint8_t>{ can::focQueryAlgorithmsId }), SentTypes());

    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_EQ((std::vector<uint8_t>{ can::focQueryAlgorithmsId, can::focQueryParameterId }), SentTypes());
}

TEST_F(TestCanCommandClient, a_refused_read_is_not_reported_and_gives_way_to_the_next)
{
    RecordSends();
    BringCompatible(1);

    Receive(FocId(services::CanPriority::response, services::canCategoryErrorResponseMessageTypeId, 1),
        Message({ can::focQueryAlgorithmsId, static_cast<uint8_t>(can::FocMotorCategoryError::busy) }));

    EXPECT_EQ(can::focQueryParameterId, sent.back().messageType);
}

TEST_F(TestCanCommandClient, an_algorithms_response_is_relayed)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnAlgorithmsReceived(1, can::FocAlgorithms{ can::FocMotorMode::speed, can::FocCurrentAlgorithm::pid, can::FocSpeedAlgorithm::adrc, can::FocPositionAlgorithm::pid }));

    Receive(FocId(services::CanPriority::response, can::focAlgorithmsResponseId, 1), Message({ 1, 0, 2, 0 }));
}

TEST_F(TestCanCommandClient, a_parameter_response_is_relayed_with_its_value_and_flags)
{
    const auto bits = std::bit_cast<uint32_t>(188.5f);
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnParameterReceived(1, can::FocParameter::speedBandwidth, can::FocParameterValue{ 188.5f, true }));

    Receive(FocId(services::CanPriority::response, can::focParameterResponseId, 1),
        Message({ static_cast<uint8_t>(can::FocParameter::speedBandwidth), can::focParameterAutomaticFlag,
            static_cast<uint8_t>(bits >> 24), static_cast<uint8_t>(bits >> 16), static_cast<uint8_t>(bits >> 8), static_cast<uint8_t>(bits) }));
}

TEST_F(TestCanCommandClient, set_parameter_carries_the_value_as_a_big_endian_float)
{
    CaptureNextPayload();
    client.Commands().SendSetParameter(1, can::FocParameter::speedBandwidth, 188.5f);

    EXPECT_EQ((std::vector<uint8_t>{ static_cast<uint8_t>(can::FocParameter::speedBandwidth), 0x43, 0x3C, 0x80, 0x00 }),
        std::vector<uint8_t>(capturedPayload.begin() + 1, capturedPayload.end()));
}

TEST_F(TestCanCommandClient, the_current_stream_rate_is_set_as_a_parameter)
{
    RecordSends();

    EXPECT_TRUE(client.Commands().SendSetParameter(1, can::FocParameter::telemetryRate, 500.0f));

    ASSERT_EQ(1u, sent.size());
    EXPECT_EQ(can::focSetParameterId, sent[0].messageType);
    EXPECT_EQ(Message({ static_cast<uint8_t>(can::FocParameter::telemetryRate), 0x43, 0xFA, 0x00, 0x00 }), AfterSequence(sent[0].payload));
    EXPECT_TRUE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, an_accepted_mode_change_reads_the_settings_again)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    client.Commands().SendSetControlMode(1, can::FocMotorMode::speed);
    sent.clear();

    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focSelectControlModeId, services::CanAckStatus::success));
    DeliverAck(can::focSelectControlModeId);

    EXPECT_EQ((std::vector<uint8_t>{ can::focQueryAlgorithmsId }), SentTypes());
}

TEST_F(TestCanCommandClient, an_accepted_alignment_reads_the_settings_again)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    client.Commands().SendAlign(1);
    sent.clear();

    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focAlignId, services::CanAckStatus::success));
    DeliverAck(can::focAlignId);
    AnswerAllReads();

    EXPECT_EQ(WritableParameterCount() + readWithTheStatus, Count(can::focQueryParameterId));
}

TEST_F(TestCanCommandClient, an_accepted_identification_reads_the_identified_motor_parameters)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    client.Commands().SendIdentifyElectrical(1);
    sent.clear();

    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focIdentifyElectricalId, services::CanAckStatus::success));
    DeliverAck(can::focIdentifyElectricalId);
    AnswerAllReads();

    EXPECT_TRUE(Queried(can::FocParameter::resistance));
    EXPECT_TRUE(Queried(can::FocParameter::inductance));
    EXPECT_TRUE(Queried(can::FocParameter::polePairs));
}

TEST_F(TestCanCommandClient, a_refused_identification_reads_no_motor_parameters)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    client.Commands().SendIdentifyElectrical(1);
    sent.clear();

    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focIdentifyElectricalId, services::CanAckStatus::invalidState));
    DeliverAck(can::focIdentifyElectricalId, 1, services::CanAckStatus::invalidState);

    EXPECT_TRUE(sent.empty());
}

TEST_F(TestCanCommandClient, an_accepted_command_reads_the_status)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    client.Commands().SendStartMotor(1);
    sent.clear();

    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focStartId, services::CanAckStatus::success));
    DeliverAck(can::focStartId);

    EXPECT_EQ((std::vector<uint8_t>{ can::focRequestTelemetryId }), SentTypes());
}

TEST_F(TestCanCommandClient, a_refused_command_reads_nothing)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    client.Commands().SendSetControlMode(1, can::FocMotorMode::speed);
    sent.clear();

    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focSelectControlModeId, services::CanAckStatus::invalidState));
    DeliverAck(can::focSelectControlModeId, 1, services::CanAckStatus::invalidState);

    EXPECT_TRUE(sent.empty());
}

TEST_F(TestCanCommandClient, the_status_of_a_compatible_node_is_read_every_second)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    sent.clear();

    ForwardTime(CanCommandClient::statusRefreshInterval);
    AnswerAllReads();
    ForwardTime(CanCommandClient::statusRefreshInterval);

    EXPECT_EQ(2u, Count(can::focRequestTelemetryId));
    EXPECT_EQ(readWithTheStatus, Count(can::focQueryParameterId));
}

TEST_F(TestCanCommandClient, each_status_asks_whether_the_target_is_reached_the_winding_temperature_and_whether_the_drive_is_homed)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    sent.clear();

    ForwardTime(CanCommandClient::statusRefreshInterval);
    AnswerAllReads();

    EXPECT_EQ((std::vector<uint8_t>{ can::focRequestTelemetryId, can::focQueryParameterId, can::focQueryParameterId, can::focQueryParameterId }), SentTypes());
    EXPECT_TRUE(Queried(can::FocParameter::targetReached));
    EXPECT_TRUE(Queried(can::FocParameter::windingTemperature));
    EXPECT_TRUE(Queried(can::FocParameter::homed));
}

TEST_F(TestCanCommandClient, a_status_read_is_not_queued_twice)
{
    RecordSends();
    BringCompatible(1);

    client.Data().RequestStatus(1);
    client.Data().RequestStatus(1);
    AnswerAllReads();

    EXPECT_EQ(1u, Count(can::focRequestTelemetryId));
}

TEST_F(TestCanCommandClient, going_offline_forgets_the_reads)
{
    RecordSends();
    BringCompatible(1);

    EXPECT_CALL(observer, OnNodeOnlineChanged(1, false));
    EXPECT_CALL(observer, OnConnectionChanged(false));
    SetBusConnected(false);
    sent.clear();

    ForwardTime(std::chrono::seconds(5));

    EXPECT_TRUE(sent.empty());
}

TEST_F(TestCanCommandClient, alignment_and_identification_wait_two_minutes_for_their_answer)
{
    client.Commands().SendAlign(1);
    client.Commands().SendIdentifyElectrical(2);

    ForwardTime(std::chrono::seconds(10));
    EXPECT_TRUE(client.IsBusy(1));

    EXPECT_CALL(observer, OnCommandTimeout(1, can::focAlignId));
    EXPECT_CALL(observer, OnCommandTimeout(2, can::focIdentifyElectricalId));
    ForwardTime(CommandTracker::motionCommandTimeout - std::chrono::seconds(10));
}

TEST_F(TestCanCommandClient, the_timeout_follows_the_command)
{
    EXPECT_EQ(CommandTracker::commandTimeout, CommandTracker::TimeoutFor(can::focSetParameterId));
    EXPECT_EQ(CommandTracker::motionCommandTimeout, CommandTracker::TimeoutFor(can::focAlignId));
}

TEST_F(TestCanCommandClient, a_start_waits_long_enough_for_the_rotor_to_rest_and_the_offsets_to_be_measured)
{
    EXPECT_EQ(std::chrono::seconds(12), CommandTracker::TimeoutFor(can::focStartId));
}

TEST_F(TestCanCommandClient, a_refresh_of_the_readings_leaves_the_homed_flag_to_the_status)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    sent.clear();

    client.Data().RequestReadings(1);
    AnswerAllReads();

    EXPECT_FALSE(Queried(can::FocParameter::homed));
}

TEST_F(TestCanCommandClient, a_refresh_of_the_readings_leaves_the_target_reached_flag_to_the_status)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    sent.clear();

    client.Data().RequestReadings(1);
    AnswerAllReads();

    EXPECT_FALSE(Queried(can::FocParameter::targetReached));
}

TEST_F(TestCanCommandClient, calibration_and_diagnostics_go_out_as_their_own_commands)
{
    RecordSends();

    EXPECT_TRUE(client.Commands().SendCalibrate(1));
    EXPECT_TRUE(client.Commands().SendClearCalibration(2));
    EXPECT_TRUE(client.Commands().SendClearDiagnostics(3));

    EXPECT_EQ((std::vector<uint8_t>{ can::focCalibrateId, can::focClearCalibrationId, can::focClearDiagnosticsId }), SentTypes());
}

TEST_F(TestCanCommandClient, a_calibration_waits_two_minutes_for_its_answer)
{
    EXPECT_EQ(CommandTracker::motionCommandTimeout, CommandTracker::TimeoutFor(can::focCalibrateId));
    EXPECT_EQ(CommandTracker::commandTimeout, CommandTracker::TimeoutFor(can::focClearCalibrationId));
    EXPECT_EQ(CommandTracker::commandTimeout, CommandTracker::TimeoutFor(can::focClearDiagnosticsId));
}

TEST_F(TestCanCommandClient, an_accepted_calibration_reads_the_settings_and_diagnostics_again)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    client.Commands().SendCalibrate(1);
    sent.clear();

    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focCalibrateId, services::CanAckStatus::success));
    DeliverAck(can::focCalibrateId);
    AnswerAllReads();

    EXPECT_EQ(can::focParameters.size(), Count(can::focQueryParameterId));
}

TEST_F(TestCanCommandClient, a_cleared_calibration_reads_the_settings_and_diagnostics_again)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    client.Commands().SendClearCalibration(1);
    sent.clear();

    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focClearCalibrationId, services::CanAckStatus::success));
    DeliverAck(can::focClearCalibrationId);
    AnswerAllReads();

    EXPECT_EQ(can::focParameters.size(), Count(can::focQueryParameterId));
}

TEST_F(TestCanCommandClient, cleared_statistics_are_read_again)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    client.Commands().SendClearDiagnostics(1);
    sent.clear();

    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focClearDiagnosticsId, services::CanAckStatus::success));
    DeliverAck(can::focClearDiagnosticsId);
    AnswerAllReads();

    EXPECT_EQ(ReadOnlyParameterCount(), Count(can::focQueryParameterId));
}

TEST_F(TestCanCommandClient, a_compatible_node_is_read_for_its_firmware_identity)
{
    RecordSends();
    BringCompatible(1);

    AnswerAllReads();

    EXPECT_TRUE(Queried(can::FocParameter::firmwareVersionMajor));
    EXPECT_TRUE(Queried(can::FocParameter::boardKind));
    EXPECT_TRUE(Queried(can::FocParameter::stagedVersionPatch));
}

TEST_F(TestCanCommandClient, the_firmware_identity_is_reported_once_every_value_arrived)
{
    BringOnline(1);
    const FirmwareIdentity identity{ { 1, 6, 0 }, 1, { 1, 7, 2 } };

    EXPECT_CALL(observer, OnParameterReceived(1, _, _)).Times(7);
    EXPECT_CALL(observer, OnFirmwareIdentityReceived(1, identity));
    DeliverIdentity(1, identity);
}

TEST_F(TestCanCommandClient, a_firmware_identity_request_reads_the_values_again_once)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    sent.clear();

    client.Data().RequestFirmwareIdentity(1);
    client.Data().RequestFirmwareIdentity(1);
    AnswerAllReads();

    EXPECT_EQ(7, Count(can::focQueryParameterId));
}

TEST_F(TestCanCommandClient, firmware_that_refuses_the_staged_version_has_no_upgrade_service)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    sent.clear();
    client.Data().RequestFirmwareIdentity(1);
    for (auto answered = 0; answered != 4; ++answered)
        AnswerLastRead();
    const FirmwareIdentity identity{ { 1, 6, 0 }, 1, {}, false };

    EXPECT_CALL(observer, OnParameterReceived(1, _, _)).Times(4);
    DeliverRunningIdentity(1, identity);
    EXPECT_CALL(observer, OnFirmwareIdentityReceived(1, identity));
    DeliverAck(can::focQueryParameterId, 1, services::CanAckStatus::invalidPayload);
}

TEST_F(TestCanCommandClient, a_refused_reading_other_than_the_staged_version_reports_no_identity)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    sent.clear();
    client.Data().RequestFirmwareIdentity(1);

    EXPECT_CALL(observer, OnParameterReceived(1, _, _)).Times(4);
    DeliverRunningIdentity(1, FirmwareIdentity{ { 1, 6, 0 }, 1, {} });
    DeliverAck(can::focQueryParameterId, 1, services::CanAckStatus::invalidPayload);
}

TEST_F(TestCanCommandClient, a_firmware_upgrade_answer_names_the_node_it_came_from)
{
    BringOnline(3);
    Receive(services::MakeCanId(services::CanPriority::response, services::firmwareUpgradeCategoryId, services::fwuVerifyResponseId, 3), Message({ 0 }));

    EXPECT_EQ(3, client.FrameOrigin().SourceNodeId());
}

TEST_F(TestCanCommandClient, a_compatible_node_is_read_for_its_eight_newest_faults)
{
    RecordSends();
    BringCompatible(1);

    AnswerAllReads();

    std::vector<uint8_t> ages;
    for (const auto& frame : sent)
        if (frame.messageType == can::focQueryFaultHistoryId && frame.payload.size() == 2)
            ages.push_back(frame.payload[1]);
    EXPECT_EQ((std::vector<uint8_t>{ 0, 1, 2, 3, 4, 5, 6, 7 }), ages);
}

TEST_F(TestCanCommandClient, a_fault_record_is_reported_once_both_of_its_responses_arrived)
{
    BringOnline(1);
    Receive(FocId(services::CanPriority::response, can::focFaultRecordResponseId, 1), Message({ 0x01, 0x02, 0x01, 0x00, 0x00, 0x05, 0xDC }));

    EXPECT_CALL(observer, OnFaultRecordReceived(1, _)).WillOnce(Invoke([](uint16_t, const can::FocFaultRecord& record)
        {
            EXPECT_EQ(1, record.age);
            EXPECT_EQ(can::FocFaultCode::overVoltage, record.code);
            EXPECT_EQ(can::FocMotorMode::speed, record.mode);
            EXPECT_EQ(1500u, record.uptimeMs);
            EXPECT_EQ(12, record.powerUp);
            EXPECT_NEAR(41.23f, record.busVoltage.Value(), 1e-4f);
            EXPECT_NEAR(-2.5f, record.peakCurrent.Value(), 1e-4f);
        }));
    Receive(FocId(services::CanPriority::response, can::focFaultConditionsResponseId, 1), Message({ 0x01, 0x00, 0x0C, 0x10, 0x1B, 0xFF, 0x06 }));
}

TEST_F(TestCanCommandClient, fault_conditions_of_another_age_are_not_reported)
{
    BringOnline(1);
    Receive(FocId(services::CanPriority::response, can::focFaultRecordResponseId, 1), Message({ 0x01, 0x02, 0x01, 0x00, 0x00, 0x05, 0xDC }));

    Receive(FocId(services::CanPriority::response, can::focFaultConditionsResponseId, 1), Message({ 0x02, 0x00, 0x0C, 0x10, 0x1B, 0xFF, 0x06 }));
}

TEST_F(TestCanCommandClient, an_accepted_clear_of_the_fault_history_reads_it_again)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    EXPECT_TRUE(client.Commands().SendClearFaultHistory(1));
    EXPECT_EQ(can::focClearFaultHistoryId, sent.back().messageType);
    sent.clear();

    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focClearFaultHistoryId, services::CanAckStatus::success));
    DeliverAck(can::focClearFaultHistoryId);
    AnswerAllReads();

    EXPECT_EQ(can::focFaultHistoryDepth, Count(can::focQueryFaultHistoryId));
}

TEST_F(TestCanCommandClient, recalibrate_and_reset_model_go_out_as_their_own_commands)
{
    RecordSends();

    EXPECT_TRUE(client.Commands().SendRecalibrate(1));
    EXPECT_TRUE(client.Commands().SendResetModel(2));

    EXPECT_EQ((std::vector<uint8_t>{ can::focRecalibrateId, can::focResetModelId }), SentTypes());
    EXPECT_EQ(CommandTracker::commandTimeout, CommandTracker::TimeoutFor(can::focRecalibrateId));
    EXPECT_EQ(CommandTracker::commandTimeout, CommandTracker::TimeoutFor(can::focResetModelId));
}

TEST_F(TestCanCommandClient, an_accepted_recalibration_reads_the_model_in_use_again)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    client.Commands().SendRecalibrate(1);
    sent.clear();

    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focRecalibrateId, services::CanAckStatus::success));
    DeliverAck(can::focRecalibrateId);
    AnswerAllReads();

    EXPECT_TRUE(Queried(can::FocParameter::modelResistance));
    EXPECT_TRUE(Queried(can::FocParameter::modelViscousFriction));
    EXPECT_TRUE(Queried(can::FocParameter::recalibrationsApplied));
    EXPECT_TRUE(Queried(can::FocParameter::recalibrationsStored));
}

TEST_F(TestCanCommandClient, an_accepted_model_reset_reads_the_model_in_use_again)
{
    RecordSends();
    BringCompatible(1);
    AnswerAllReads();
    client.Commands().SendResetModel(1);
    sent.clear();

    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focResetModelId, services::CanAckStatus::success));
    DeliverAck(can::focResetModelId);
    AnswerAllReads();

    EXPECT_TRUE(Queried(can::FocParameter::modelInductance));
    EXPECT_TRUE(Queried(can::FocParameter::modelInertia));
}
