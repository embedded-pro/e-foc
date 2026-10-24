#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "tools/can_commander/logic/test/CanBusAdapterMock.hpp"
#include "tools/can_commander/logic/test/CanCommandClientObserverMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <limits>
#include <set>

namespace
{
    using namespace tool;
    using testing::_;
    using testing::Invoke;
    using testing::StrictMock;

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

        void DeliverContract(uint8_t major, uint16_t node)
        {
            Receive(FocId(services::CanPriority::response, can::focContractVersionResponseId, node), Message({ major, 0 }));
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
            client.SendStartMotor(node);
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
    client.SendStartMotor(1);

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

    LogReceived(services::MakeCanId(services::CanPriority::response, can::focMotorCategoryId, can::focMotorTypeResponseId, 3), Message({ 0 }));
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
    client.SendStartMotor(1);

    EXPECT_CALL(observer, OnNodeOnlineChanged(1, false));
    ForwardTime(std::chrono::seconds(1));
    EXPECT_FALSE(client.IsBusy(1));

    ForwardTime(std::chrono::seconds(2));
}

TEST_F(TestCanCommandClient, closing_the_bus_reports_nodes_offline_and_ends_pending_commands_at_once)
{
    BringOnline(1);
    client.SendStartMotor(1);

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
    const auto statusId = FocId(services::CanPriority::telemetry, can::focMotorTypeResponseId, 1);
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
    EXPECT_TRUE(client.SendStartMotor(1));
    EXPECT_TRUE(client.SendStopMotor(2));
    EXPECT_TRUE(client.SendEmergencyStop(3));
    EXPECT_TRUE(client.SendSetControlMode(4, can::FocMotorMode::speed));
    EXPECT_TRUE(client.SendSetTorqueSetpoint(5, 5.0f));
    EXPECT_TRUE(client.SendSetSpeedSetpoint(6, 100.0f));
    EXPECT_TRUE(client.SendSetPositionSetpoint(7, 1.0f));
    EXPECT_TRUE(client.SendSetCurrentBandwidth(8, 500.0f));
    EXPECT_TRUE(client.SendSetSpeedBandwidth(9, 188.5f));
    EXPECT_TRUE(client.SendSetPositionBandwidth(10, 18.8f));

    for (uint16_t node = 1; node <= 10; ++node)
        EXPECT_TRUE(client.IsBusy(node)) << node;
}

TEST_F(TestCanCommandClient, overrange_setpoints_are_still_sent_and_pending)
{
    client.SendSetSpeedSetpoint(1, 100000.0f);
    client.SendSetSpeedSetpoint(2, -100000.0f);
    client.SendSetPositionSetpoint(3, 1000.0f);
    client.SendSetPositionSetpoint(4, -1000.0f);

    for (uint16_t node = 1; node <= 4; ++node)
        EXPECT_TRUE(client.IsBusy(node)) << node;
}

TEST_F(TestCanCommandClient, request_data_does_not_make_the_node_busy)
{
    EXPECT_TRUE(client.RequestData(1));
    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, an_acknowledgement_ends_the_pending_command)
{
    testing::InSequence sequence;
    EXPECT_CALL(observer, OnBusyChanged(1, true));
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnBusyChanged(1, false));
    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focStartId, services::CanAckStatus::success));

    client.SendStartMotor(1);
    DeliverAck(can::focStartId);

    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, an_acknowledgement_from_another_node_leaves_the_command_pending)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(2, true));
    EXPECT_CALL(observer, OnCommandAck(2, can::focMotorCategoryId, can::focStartId, services::CanAckStatus::success));

    client.SendStartMotor(1);
    DeliverAck(can::focStartId, 2);

    EXPECT_TRUE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, a_late_acknowledgement_of_an_older_command_keeps_the_newer_one_pending)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focStartId, _));
    EXPECT_CALL(observer, OnCommandAck(1, can::focMotorCategoryId, can::focEmergencyStopId, _));

    client.SendStartMotor(1);
    client.SendEmergencyStop(1);

    DeliverAck(can::focStartId);
    EXPECT_TRUE(client.IsBusy(1));

    DeliverAck(can::focEmergencyStopId);
    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, a_rejection_reports_the_reason_and_ends_the_pending_command)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnCommandRejected(1, can::focSetTorqueSetpointId, can::FocMotorCategoryError::modeMismatch));

    client.SendSetTorqueSetpoint(1, 2.0f);
    Receive(FocId(services::CanPriority::response, services::canCategoryErrorResponseMessageTypeId, 1),
        Message({ can::focSetTorqueSetpointId, static_cast<uint8_t>(can::FocMotorCategoryError::modeMismatch) }));

    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, a_pending_command_times_out_after_the_command_timeout)
{
    client.SendStartMotor(1);

    ForwardTime(CanCommandClient::commandTimeout - std::chrono::milliseconds(1));
    EXPECT_TRUE(client.IsBusy(1));

    EXPECT_CALL(observer, OnCommandTimeout(1));
    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, a_newer_command_restarts_the_timeout)
{
    client.SendStartMotor(1);
    ForwardTime(std::chrono::milliseconds(1500));
    client.SendEmergencyStop(1);

    ForwardTime(std::chrono::milliseconds(1500));
    EXPECT_TRUE(client.IsBusy(1));

    EXPECT_CALL(observer, OnCommandTimeout(1));
    ForwardTime(std::chrono::milliseconds(500));
}

TEST_F(TestCanCommandClient, an_incompatible_node_refuses_commands_without_touching_its_pending_command)
{
    BringOnline(1);
    client.SendStartMotor(1);

    EXPECT_CALL(observer, OnContractVersion(1, static_cast<uint8_t>(can::focContractVersionMajor + 1), 0, false));
    DeliverContract(static_cast<uint8_t>(can::focContractVersionMajor + 1), 1);

    EXPECT_EQ(can::ContractCompatibility::incompatible, client.Compatibility(1));
    EXPECT_FALSE(client.SendEmergencyStop(1));
    EXPECT_TRUE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, an_incompatible_node_does_not_block_the_others)
{
    BringOnline(1);
    BringOnline(2);
    EXPECT_CALL(observer, OnContractVersion(1, _, _, false));
    EXPECT_CALL(observer, OnContractVersion(2, can::focContractVersionMajor, 0, true));

    DeliverContract(static_cast<uint8_t>(can::focContractVersionMajor + 1), 1);
    DeliverContract(can::focContractVersionMajor, 2);

    EXPECT_FALSE(client.SendEmergencyStop(1));
    EXPECT_TRUE(client.SendEmergencyStop(2));
    EXPECT_EQ(can::ContractCompatibility::compatible, client.Compatibility(2));
}

TEST_F(TestCanCommandClient, telemetry_status_notifies_motor_status_and_speed_position)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnMotorStatusReceived(1, FocMotorState::running, FocFaultCode::none));
    EXPECT_CALL(observer, OnSpeedPositionReceived(1, testing::FloatNear(10.0f, 0.01f), testing::FloatNear(1.0f, 0.001f)));

    const auto data = Message({ static_cast<uint8_t>(FocMotorState::running), static_cast<uint8_t>(FocFaultCode::none), 0, 10, 0, 100 });
    const auto rawId = FocId(services::CanPriority::telemetry, can::focTelemetryStatusResponseId, 1);
    LogReceived(rawId, data);
    Receive(rawId, data);
}

TEST_F(TestCanCommandClient, telemetry_is_attributed_to_the_node_that_sent_it)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(2, true));
    EXPECT_CALL(observer, OnNodeOnlineChanged(3, true));
    EXPECT_CALL(observer, OnMotorStatusReceived(2, FocMotorState::running, FocFaultCode::none));
    EXPECT_CALL(observer, OnMotorStatusReceived(3, FocMotorState::idle, FocFaultCode::none));
    EXPECT_CALL(observer, OnSpeedPositionReceived(2, _, _));
    EXPECT_CALL(observer, OnSpeedPositionReceived(3, _, _));

    Receive(FocId(services::CanPriority::telemetry, can::focTelemetryStatusResponseId, 2),
        Message({ static_cast<uint8_t>(FocMotorState::running), 0, 0, 0, 0, 0 }));
    Receive(FocId(services::CanPriority::telemetry, can::focTelemetryStatusResponseId, 3),
        Message({ static_cast<uint8_t>(FocMotorState::idle), 0, 0, 0, 0, 0 }));
}

TEST_F(TestCanCommandClient, telemetry_status_with_fault_notifies_fault_event)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnMotorStatusReceived(1, FocMotorState::fault, FocFaultCode::overCurrent));
    EXPECT_CALL(observer, OnSpeedPositionReceived(1, _, _));
    EXPECT_CALL(observer, OnFaultEventReceived(1, FocFaultCode::overCurrent));

    Receive(FocId(services::CanPriority::telemetry, can::focTelemetryStatusResponseId, 1),
        Message({ static_cast<uint8_t>(FocMotorState::fault), static_cast<uint8_t>(FocFaultCode::overCurrent), 0, 0, 0, 0 }));
}

TEST_F(TestCanCommandClient, telemetry_electrical_notifies_current_and_voltage)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnCurrentMeasurementReceived(1, testing::FloatNear(3.0f, 0.01f), testing::FloatNear(5.0f, 0.01f)));
    EXPECT_CALL(observer, OnBusVoltageReceived(1, testing::FloatNear(24.0f, 0.01f)));

    Receive(FocId(services::CanPriority::telemetry, can::focTelemetryElectricalResponseId, 1), Message({ 0, 240, 0, 0, 0, 50, 0, 30 }));
}

TEST_F(TestCanCommandClient, motor_type_response_received_does_not_call_any_observer)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));

    Receive(FocId(services::CanPriority::response, can::focMotorTypeResponseId, 1), Message({ static_cast<uint8_t>(can::FocMotorMode::speed) }));
}

TEST_F(TestCanCommandClient, electrical_params_response_notifies_observer)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnElectricalParametersReceived(1, testing::FloatNear(0.5f, 0.001f), testing::FloatNear(1.0f, 0.001f), 7));

    Receive(FocId(services::CanPriority::response, can::focElectricalParamsResponseId, 1), Message({ 0x01, 0xF4, 0x03, 0xE8, 7 }));
}

TEST_F(TestCanCommandClient, mechanical_params_response_notifies_observer)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnMechanicalParametersReceived(1, testing::FloatNear(1.5e-5f, 1e-7f), testing::FloatNear(7.06e-6f, 1e-8f)));

    Receive(FocId(services::CanPriority::response, can::focMechanicalParamsResponseId, 1), Message({ 0x00, 0x00, 0x3A, 0x98, 0x00, 0x00, 0x1B, 0x94 }));
}

TEST_F(TestCanCommandClient, select_control_mode_response_acknowledges_the_mode_and_ends_the_command)
{
    EXPECT_CALL(observer, OnNodeOnlineChanged(1, true));
    EXPECT_CALL(observer, OnControlModeAcknowledged(1, can::FocMotorMode::speed));

    client.SendSetControlMode(1, can::FocMotorMode::speed);
    Receive(FocId(services::CanPriority::response, can::focSelectControlModeResponseId, 1), Message({ static_cast<uint8_t>(can::FocMotorMode::speed) }));

    EXPECT_FALSE(client.IsBusy(1));
}

TEST_F(TestCanCommandClient, send_torque_setpoint_encodes_with_correct_scale)
{
    CaptureNextPayload();
    client.SendSetTorqueSetpoint(1, 5.0f); // 5.0 A * focCurrentScale(10) = 50 = 0x0032

    ASSERT_EQ(capturedPayload.size(), 3u);
    EXPECT_EQ(capturedPayload[1], 0x00u);
    EXPECT_EQ(capturedPayload[2], 0x32u);
}

TEST_F(TestCanCommandClient, send_speed_setpoint_overrange_wire_value_clamped_to_int16_max)
{
    CaptureNextPayload();
    client.SendSetSpeedSetpoint(1, 100000.0f);

    ASSERT_EQ(capturedPayload.size(), 3u);
    EXPECT_EQ(capturedPayload[1], 0x7Fu);
    EXPECT_EQ(capturedPayload[2], 0xFFu);
}

TEST_F(TestCanCommandClient, send_speed_setpoint_overrange_negative_wire_value_clamped_to_int16_min)
{
    CaptureNextPayload();
    client.SendSetSpeedSetpoint(1, -100000.0f);

    ASSERT_EQ(capturedPayload.size(), 3u);
    EXPECT_EQ(capturedPayload[1], 0x80u);
    EXPECT_EQ(capturedPayload[2], 0x00u);
}

TEST_F(TestCanCommandClient, send_speed_setpoint_extreme_value_clamps_without_ub)
{
    CaptureNextPayload();
    client.SendSetSpeedSetpoint(1, 1e10f);

    ASSERT_EQ(capturedPayload.size(), 3u);
    EXPECT_EQ(capturedPayload[1], 0x7Fu);
    EXPECT_EQ(capturedPayload[2], 0xFFu);
}

TEST_F(TestCanCommandClient, send_speed_setpoint_nan_encodes_as_zero)
{
    CaptureNextPayload();
    client.SendSetSpeedSetpoint(1, std::numeric_limits<float>::quiet_NaN());

    ASSERT_EQ(capturedPayload.size(), 3u);
    EXPECT_EQ(capturedPayload[1], 0x00u);
    EXPECT_EQ(capturedPayload[2], 0x00u);
}

TEST_F(TestCanCommandClient, send_position_setpoint_encodes_with_correct_scale)
{
    CaptureNextPayload();
    client.SendSetPositionSetpoint(1, 0.1f); // 0.1 * focPositionScale(100) = 10 = 0x000A

    ASSERT_EQ(capturedPayload.size(), 3u);
    EXPECT_EQ(capturedPayload[1], 0x00u);
    EXPECT_EQ(capturedPayload[2], 0x0Au);
}

TEST_F(TestCanCommandClient, send_position_setpoint_rounds_to_nearest_wire_value)
{
    CaptureNextPayload();
    client.SendSetPositionSetpoint(1, 0.157f); // 15.7 → 16, truncation would give 15

    ASSERT_EQ(capturedPayload.size(), 3u);
    EXPECT_EQ(capturedPayload[1], 0x00u);
    EXPECT_EQ(capturedPayload[2], 0x10u);
}

TEST_F(TestCanCommandClientAdapterFails, every_command_stays_pending_when_the_adapter_reports_failure)
{
    client.SendStartMotor(1);
    client.SendStopMotor(2);
    client.SendEmergencyStop(3);
    client.SendSetControlMode(4, can::FocMotorMode::speed);
    client.SendSetTorqueSetpoint(5, 5.0f);
    client.SendSetSpeedSetpoint(6, 10.0f);
    client.SendSetPositionSetpoint(7, 1.0f);
    client.SendSetCurrentBandwidth(8, 500.0f);
    client.SendSetSpeedBandwidth(9, 188.5f);
    client.SendSetPositionBandwidth(10, 18.8f);

    for (uint16_t node = 1; node <= 10; ++node)
        EXPECT_TRUE(client.IsBusy(node)) << node;
}

TEST_F(TestCanCommandClientAdapterFails, timeout_after_failed_send_ends_the_pending_command)
{
    client.SendStartMotor(1);

    EXPECT_CALL(observer, OnCommandTimeout(1));
    ForwardTime(CanCommandClient::commandTimeout);

    EXPECT_FALSE(client.IsBusy(1));
}
