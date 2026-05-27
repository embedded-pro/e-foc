#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "tools/can_commander/logic/test/CanBusAdapterMock.hpp"
#include "tools/can_commander/logic/test/CanCommandClientObserverMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace tool;
    using namespace services;
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
                infra::Function<void(hal::Can::Id, const hal::Can::Message&)>& receiveCallback)
            {
                EXPECT_CALL(adapter, ReceiveData(_)).WillOnce([&receiveCallback](const auto& callback)
                    {
                        receiveCallback = callback;
                    });
                EXPECT_CALL(adapter, SendData(_, _, _))
                    .Times(testing::AnyNumber())
                    .WillRepeatedly(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                        {
                            cb(true);
                        }));
            }
        };

        void SetUp() override
        {
            EXPECT_CALL(observer, OnBusyChanged(_)).Times(testing::AnyNumber());
        }

        StrictMock<CanBusAdapterMock> adapter;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receiveCallback;
        FixtureInit fixtureInit{ adapter, receiveCallback };
        CanCommandClient client{ adapter };
        StrictMock<CanCommandClientObserverMock> observer{ client };
    };

    // ---------- Initial state ----------

    TEST_F(TestCanCommandClient, initially_not_busy)
    {
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, default_node_id_is_one)
    {
        EXPECT_EQ(client.NodeId(), 1u);
    }

    // ---------- Node ID ----------

    TEST_F(TestCanCommandClient, set_node_id_is_reflected)
    {
        client.SetNodeId(42);
        EXPECT_EQ(client.NodeId(), 42u);
    }

    // ---------- Timeout ----------

    TEST_F(TestCanCommandClient, handle_timeout_clears_busy_and_notifies)
    {
        EXPECT_CALL(observer, OnBusyChanged(false)).Times(testing::AnyNumber());
        EXPECT_CALL(observer, OnCommandTimeout());

        client.HandleTimeout();

        EXPECT_FALSE(client.IsBusy());
    }

    // ---------- Adapter event relay ----------

    TEST_F(TestCanCommandClient, frame_log_relayed_from_adapter)
    {
        CanFrame data;
        data.push_back(0x42);

        EXPECT_CALL(observer, OnFrameLog(false, 0x12345u, _));

        adapter.NotifyObservers([&data](auto& obs)
            {
                obs.OnFrameLog(false, 0x12345u, data);
            });
    }

    TEST_F(TestCanCommandClient, connection_changed_relayed)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));

        adapter.NotifyObservers([](auto& obs)
            {
                obs.OnConnectionChanged(true);
            });
    }

    TEST_F(TestCanCommandClient, adapter_error_relayed)
    {
        EXPECT_CALL(observer, OnAdapterError(_));

        adapter.NotifyObservers([](auto& obs)
            {
                obs.OnError("test error");
            });
    }

    // ---------- Command busy management ----------

    TEST_F(TestCanCommandClient, send_start_motor_clears_busy)
    {
        client.SendStartMotor();
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_speed_setpoint_clears_busy)
    {
        client.SendSetSpeedSetpoint(100.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_position_setpoint_clears_busy)
    {
        client.SendSetPositionSetpoint(1.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_torque_setpoint_clears_busy)
    {
        client.SendSetTorqueSetpoint(5.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    // ---------- Setpoint clamping ----------

    TEST_F(TestCanCommandClient, send_speed_setpoint_overrange_positive_clears_busy)
    {
        client.SendSetSpeedSetpoint(100000.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_speed_setpoint_overrange_negative_clears_busy)
    {
        client.SendSetSpeedSetpoint(-100000.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_position_setpoint_overrange_positive_clears_busy)
    {
        client.SendSetPositionSetpoint(1000.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_position_setpoint_overrange_negative_clears_busy)
    {
        client.SendSetPositionSetpoint(-1000.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    // ---------- Send commands ----------

    TEST_F(TestCanCommandClient, send_stop_motor_clears_busy)
    {
        client.SendStopMotor();
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_emergency_stop_clears_busy)
    {
        client.SendEmergencyStop();
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_set_control_mode_clears_busy)
    {
        client.SendSetControlMode(FocMotorMode::speed);
        EXPECT_FALSE(client.IsBusy());
    }

    // ---------- PID setters ----------

    TEST_F(TestCanCommandClient, send_set_current_id_pid_clears_busy)
    {
        client.SendSetCurrentIdPid(1.0f, 0.1f, 0.01f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_set_current_iq_pid_clears_busy)
    {
        client.SendSetCurrentIqPid(1.0f, 0.1f, 0.01f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_set_speed_pid_clears_busy)
    {
        client.SendSetSpeedPid(1.0f, 0.1f, 0.01f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_set_position_pid_clears_busy)
    {
        client.SendSetPositionPid(1.0f, 0.1f, 0.01f);
        EXPECT_FALSE(client.IsBusy());
    }

    // ---------- Request data ----------

    TEST_F(TestCanCommandClient, request_data_does_not_change_busy_state)
    {
        client.RequestData();
        EXPECT_FALSE(client.IsBusy());
    }

    // ---------- Busy notifications ----------

    TEST_F(TestCanCommandClient, send_command_notifies_busy_true_then_false)
    {
        testing::InSequence seq;
        EXPECT_CALL(observer, OnBusyChanged(true));
        EXPECT_CALL(observer, OnBusyChanged(false));

        client.SendStartMotor();
    }

    // ---------- Server online / offline ----------

    TEST_F(TestCanCommandClient, server_online_event_forwarded_when_frame_received_from_node)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));

        hal::Can::Message emptyPayload;
        auto canId = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::heartbeat,
                canSystemCategoryId,
                canHeartbeatMessageTypeId,
                1));
        receiveCallback(canId, emptyPayload);
    }

    TEST_F(TestCanCommandClient, server_offline_event_forwarded_after_timeout)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));

        hal::Can::Message emptyPayload;
        auto canId = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::heartbeat,
                canSystemCategoryId,
                canHeartbeatMessageTypeId,
                1));
        receiveCallback(canId, emptyPayload);

        EXPECT_CALL(observer, OnConnectionChanged(false));

        ForwardTime(std::chrono::seconds(4));
    }

    // ---------- Telemetry ----------

    TEST_F(TestCanCommandClient, telemetry_status_notifies_motor_status_and_speed_position)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));
        EXPECT_CALL(observer, OnMotorStatusReceived(FocMotorState::running, FocFaultCode::none));
        EXPECT_CALL(observer, OnSpeedPositionReceived(testing::FloatNear(1.0f, 0.01f), testing::FloatNear(0.1f, 0.01f)));

        hal::Can::Message data;
        data.resize(6, 0);
        data[0] = static_cast<uint8_t>(FocMotorState::running);
        data[1] = static_cast<uint8_t>(FocFaultCode::none);
        data[2] = 0;
        data[3] = 100; // speed wire = 100 → physical = 100 / 10 = 10.0 rad/s
        data[4] = 3;
        data[5] = 232; // position wire = 1000 → physical = 1000 / 1000 = 1.0 rad

        auto canId = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::telemetry,
                focMotorCategoryId,
                focTelemetryStatusResponseId,
                1));
        receiveCallback(canId, data);
    }

    TEST_F(TestCanCommandClient, telemetry_status_with_fault_notifies_fault_event)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));
        EXPECT_CALL(observer, OnMotorStatusReceived(FocMotorState::fault, FocFaultCode::overCurrent));
        EXPECT_CALL(observer, OnSpeedPositionReceived(_, _));
        EXPECT_CALL(observer, OnFaultEventReceived(FocFaultCode::overCurrent));

        hal::Can::Message data;
        data.resize(6, 0);
        data[0] = static_cast<uint8_t>(FocMotorState::fault);
        data[1] = static_cast<uint8_t>(FocFaultCode::overCurrent);
        data[2] = 0;
        data[3] = 0;
        data[4] = 0;
        data[5] = 0;

        auto canId = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::telemetry,
                focMotorCategoryId,
                focTelemetryStatusResponseId,
                1));
        receiveCallback(canId, data);
    }

    TEST_F(TestCanCommandClient, telemetry_electrical_notifies_current_and_voltage)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));
        EXPECT_CALL(observer, OnCurrentMeasurementReceived(testing::FloatNear(0.3f, 0.01f),
                                  testing::FloatNear(0.5f, 0.01f)));
        EXPECT_CALL(observer, OnBusVoltageReceived(testing::FloatNear(24.0f, 0.01f)));

        hal::Can::Message data;
        data.resize(8, 0);
        data[0] = 0;
        data[1] = 240; // voltage wire = 240 → physical = 240 / 10 = 24.0 V
        data[2] = 0;
        data[3] = 0;
        data[4] = 1;
        data[5] = 244; // iq wire = 500 → physical = 500 / 100 = 5.0 A
        data[6] = 1;
        data[7] = 44; // id wire = 300 → physical = 300 / 100 = 3.0 A

        auto canId = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::telemetry,
                focMotorCategoryId,
                focTelemetryElectricalResponseId,
                1));
        receiveCallback(canId, data);
    }

    TEST_F(TestCanCommandClient, motor_type_response_received_does_not_call_any_observer)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));

        hal::Can::Message data;
        data.push_back(static_cast<uint8_t>(FocMotorMode::speed));

        auto canId = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::response,
                focMotorCategoryId,
                focMotorTypeResponseId,
                1));
        receiveCallback(canId, data);
        // StrictMock: no observer method should be called — OnMotorTypeResponse is a no-op
    }

    TEST_F(TestCanCommandClient, electrical_params_response_received_does_not_call_any_observer)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));

        hal::Can::Message data;
        data.resize(4, 0);

        auto canId = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::response,
                focMotorCategoryId,
                focElectricalParamsResponseId,
                1));
        receiveCallback(canId, data);
        // StrictMock: no observer method should be called — OnElectricalParamsResponse is a no-op
    }

    TEST_F(TestCanCommandClient, mechanical_params_response_received_does_not_call_any_observer)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));

        hal::Can::Message data;
        data.resize(4, 0);

        auto canId = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::response,
                focMotorCategoryId,
                focMechanicalParamsResponseId,
                1));
        receiveCallback(canId, data);
        // StrictMock: no observer method should be called — OnMechanicalParamsResponse is a no-op
    }

    // ---------- SelectControlModeResponse forwarding ----------

    TEST_F(TestCanCommandClient, select_control_mode_response_ok_notifies_control_mode_acknowledged)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));
        EXPECT_CALL(observer, OnControlModeAcknowledged(FocMotorMode::speed, FocRejectReason::ok));

        hal::Can::Message data;
        data.resize(2, 0);
        data[0] = static_cast<uint8_t>(FocMotorMode::speed);
        data[1] = static_cast<uint8_t>(FocRejectReason::ok);

        auto canId = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::response,
                focMotorCategoryId,
                focSelectControlModeResponseId,
                1));
        receiveCallback(canId, data);
    }

    TEST_F(TestCanCommandClient, select_control_mode_response_rejected_notifies_acknowledged_with_reason)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));
        EXPECT_CALL(observer, OnControlModeAcknowledged(FocMotorMode::speed, FocRejectReason::busy));

        hal::Can::Message data;
        data.resize(2, 0);
        data[0] = static_cast<uint8_t>(FocMotorMode::speed);
        data[1] = static_cast<uint8_t>(FocRejectReason::busy);

        auto canId = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::response,
                focMotorCategoryId,
                focSelectControlModeResponseId,
                1));
        receiveCallback(canId, data);
    }

    // ---------- CommandRejectedResponse forwarding ----------

    TEST_F(TestCanCommandClient, command_rejected_response_notifies_observer)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));
        EXPECT_CALL(observer, OnCommandRejected(0x10u, FocRejectReason::controlModeMismatch));

        hal::Can::Message data;
        data.resize(2, 0);
        data[0] = 0x10;
        data[1] = static_cast<uint8_t>(FocRejectReason::controlModeMismatch);

        auto canId = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::response,
                focMotorCategoryId,
                focCommandRejectedResponseId,
                1));
        receiveCallback(canId, data);
    }

    TEST_F(TestCanCommandClient, command_rejected_response_leaves_client_not_busy)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));
        EXPECT_CALL(observer, OnCommandRejected(_, _));

        hal::Can::Message data;
        data.resize(2, 0);
        data[0] = 0x01;
        data[1] = static_cast<uint8_t>(FocRejectReason::busy);

        auto canId = hal::Can::Id::Create29BitId(
            MakeCanId(CanPriority::response,
                focMotorCategoryId,
                focCommandRejectedResponseId,
                1));
        receiveCallback(canId, data);

        EXPECT_FALSE(client.IsBusy());
    }

    // ---------- Encoding: torque setpoint uses focCurrentScale ----------

    TEST_F(TestCanCommandClient, send_torque_setpoint_encodes_with_correct_scale)
    {
        hal::Can::Message capturedData;
        EXPECT_CALL(adapter, SendData(_, _, _))
            .WillOnce(Invoke([&capturedData](hal::Can::Id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                {
                    capturedData = msg;
                    cb(true);
                }));

        client.SendSetTorqueSetpoint(5.0f); // 5.0 A * focCurrentScale(100) = 500 = 0x01F4

        ASSERT_EQ(capturedData.size(), 3u);
        EXPECT_EQ(capturedData[1], 0x01u); // high byte of 500
        EXPECT_EQ(capturedData[2], 0xF4u); // low  byte of 500
    }

    // ---------- Encoding: speed setpoint clamps to INT16_MAX when overrange ----------

    TEST_F(TestCanCommandClient, send_speed_setpoint_overrange_wire_value_clamped_to_int16_max)
    {
        hal::Can::Message capturedData;
        EXPECT_CALL(adapter, SendData(_, _, _))
            .WillOnce(Invoke([&capturedData](hal::Can::Id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                {
                    capturedData = msg;
                    cb(true);
                }));

        client.SendSetSpeedSetpoint(100000.0f); // 100000 * 10 = 1000000 > INT16_MAX → clamped to 32767 = 0x7FFF

        ASSERT_EQ(capturedData.size(), 3u);
        EXPECT_EQ(capturedData[1], 0x7Fu); // high byte of INT16_MAX
        EXPECT_EQ(capturedData[2], 0xFFu); // low  byte of INT16_MAX
    }

    // ---------- Adapter send failure: busy stays true ----------

    class TestCanCommandClientAdapterFails
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        struct FailingFixtureInit
        {
            FailingFixtureInit(StrictMock<CanBusAdapterMock>& adapter,
                infra::Function<void(hal::Can::Id, const hal::Can::Message&)>& receiveCallback)
            {
                EXPECT_CALL(adapter, ReceiveData(_)).WillOnce([&receiveCallback](const auto& callback)
                    {
                        receiveCallback = callback;
                    });
                EXPECT_CALL(adapter, SendData(_, _, _))
                    .Times(testing::AnyNumber())
                    .WillRepeatedly(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                        {
                            cb(false);
                        }));
            }
        };

        void SetUp() override
        {
            EXPECT_CALL(observer, OnBusyChanged(_)).Times(testing::AnyNumber());
        }

        StrictMock<CanBusAdapterMock> adapter;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receiveCallback;
        FailingFixtureInit fixtureInit{ adapter, receiveCallback };
        CanCommandClient client{ adapter };
        StrictMock<CanCommandClientObserverMock> observer{ client };
    };

    TEST_F(TestCanCommandClientAdapterFails, send_start_clears_busy_even_when_adapter_reports_failure)
    {
        client.SendStartMotor();
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClientAdapterFails, send_stop_clears_busy_even_when_adapter_reports_failure)
    {
        client.SendStopMotor();
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClientAdapterFails, send_emergency_stop_clears_busy_even_when_adapter_reports_failure)
    {
        client.SendEmergencyStop();
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClientAdapterFails, send_set_control_mode_clears_busy_even_when_adapter_reports_failure)
    {
        client.SendSetControlMode(FocMotorMode::speed);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClientAdapterFails, send_set_torque_setpoint_clears_busy_even_when_adapter_reports_failure)
    {
        client.SendSetTorqueSetpoint(5.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClientAdapterFails, send_set_speed_setpoint_clears_busy_even_when_adapter_reports_failure)
    {
        client.SendSetSpeedSetpoint(10.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClientAdapterFails, send_set_position_setpoint_clears_busy_even_when_adapter_reports_failure)
    {
        client.SendSetPositionSetpoint(1.0f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClientAdapterFails, send_set_current_id_pid_clears_busy_even_when_adapter_reports_failure)
    {
        client.SendSetCurrentIdPid(1.0f, 0.1f, 0.01f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClientAdapterFails, send_set_current_iq_pid_clears_busy_even_when_adapter_reports_failure)
    {
        client.SendSetCurrentIqPid(1.0f, 0.1f, 0.01f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClientAdapterFails, send_set_speed_pid_clears_busy_even_when_adapter_reports_failure)
    {
        client.SendSetSpeedPid(1.0f, 0.1f, 0.01f);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClientAdapterFails, send_set_position_pid_clears_busy_even_when_adapter_reports_failure)
    {
        client.SendSetPositionPid(1.0f, 0.1f, 0.01f);
        EXPECT_FALSE(client.IsBusy());
    }
}
