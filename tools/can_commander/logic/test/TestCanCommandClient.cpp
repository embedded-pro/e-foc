#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "hal/interfaces/Can.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "tools/can_commander/logic/test/CanBusAdapterMock.hpp"
#include "tools/can_commander/logic/test/CanCommandClientObserverMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

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

    TEST_F(TestCanCommandClient, category_error_received_does_not_call_any_observer)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));

        hal::Can::Message data;
        data.push_back(can::focStartId);
        data.push_back(static_cast<uint8_t>(can::FocMotorCategoryError::busy));

        auto canId = hal::Can::Id::Create29BitId(
            services::MakeCanId(services::CanPriority::response,
                can::focMotorCategoryId,
                services::canCategoryErrorResponseMessageTypeId,
                1));
        receiveCallback(canId, data);
        // StrictMock: OnCategoryError is a no-op — no observer method should fire
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
        client.SendSetControlMode(can::FocMotorMode::speed);
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
            services::MakeCanId(services::CanPriority::heartbeat,
                services::canSystemCategoryId,
                services::canHeartbeatMessageTypeId,
                1));
        receiveCallback(canId, emptyPayload);
    }

    TEST_F(TestCanCommandClient, server_offline_event_forwarded_after_timeout)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));

        hal::Can::Message emptyPayload;
        auto canId = hal::Can::Id::Create29BitId(
            services::MakeCanId(services::CanPriority::heartbeat,
                services::canSystemCategoryId,
                services::canHeartbeatMessageTypeId,
                1));
        receiveCallback(canId, emptyPayload);

        EXPECT_CALL(observer, OnConnectionChanged(false));

        ForwardTime(std::chrono::seconds(4));
    }

    TEST_F(TestCanCommandClient, command_ack_frame_forwards_to_observer_and_keeps_busy_false)
    {
        EXPECT_CALL(observer, OnCommandAck(can::focMotorCategoryId, can::focStartId, services::CanAckStatus::success));

        CanFrame ackData;
        ackData.push_back(can::focMotorCategoryId);
        ackData.push_back(can::focStartId);
        ackData.push_back(static_cast<uint8_t>(services::CanAckStatus::success));
        ackData.push_back(0);

        const uint32_t rawId = services::MakeCanId(services::CanPriority::response,
            services::canSystemCategoryId,
            services::canCommandAckMessageTypeId,
            1);

        adapter.NotifyObservers([rawId, &ackData](auto& obs)
            {
                obs.OnFrameLog(false, rawId, ackData);
            });

        EXPECT_FALSE(client.IsBusy());
    }

    // ---------- Telemetry ----------

    TEST_F(TestCanCommandClient, telemetry_status_notifies_motor_status_and_speed_position)
    {
        EXPECT_CALL(observer, OnMotorStatusReceived(FocMotorState::running, FocFaultCode::none));
        EXPECT_CALL(observer, OnSpeedPositionReceived(testing::FloatNear(10.0f, 0.01f), testing::FloatNear(1.0f, 0.001f)));

        CanFrame data;
        data.resize(6, 0);
        data[0] = static_cast<uint8_t>(FocMotorState::running);
        data[1] = static_cast<uint8_t>(FocFaultCode::none);
        data[2] = 0;
        data[3] = 10; // speed wire = 10 → physical = 10 / focSpeedScale(1) = 10.0 rad/s
        data[4] = 0;
        data[5] = 100; // position wire = 100 → physical = 100 / focPositionScale(100) = 1.0 rad

        const uint32_t rawId = services::MakeCanId(services::CanPriority::telemetry,
            can::focMotorCategoryId,
            can::focTelemetryStatusResponseId,
            1);

        adapter.NotifyObservers([rawId, &data](auto& obs)
            {
                obs.OnFrameLog(false, rawId, data);
            });
    }

    TEST_F(TestCanCommandClient, telemetry_status_with_fault_notifies_fault_event)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));
        EXPECT_CALL(observer, OnMotorStatusReceived(FocMotorState::fault, FocFaultCode::overCurrent));
        EXPECT_CALL(observer, OnSpeedPositionReceived(_, _));
        EXPECT_CALL(observer, OnFaultEventReceived(FocFaultCode::overCurrent));

        hal::Can::Message data;
        data.resize(6, 0);
        data[0] = static_cast<uint8_t>(tool::FocMotorState::fault);
        data[1] = static_cast<uint8_t>(tool::FocFaultCode::overCurrent);
        data[2] = 0;
        data[3] = 0;
        data[4] = 0;
        data[5] = 0;

        auto canId = hal::Can::Id::Create29BitId(
            services::MakeCanId(services::CanPriority::telemetry,
                can::focMotorCategoryId,
                can::focTelemetryStatusResponseId,
                1));
        receiveCallback(canId, data);
    }

    TEST_F(TestCanCommandClient, telemetry_electrical_notifies_current_and_voltage)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));
        EXPECT_CALL(observer, OnCurrentMeasurementReceived(testing::FloatNear(3.0f, 0.01f),
                                  testing::FloatNear(5.0f, 0.01f)));
        EXPECT_CALL(observer, OnBusVoltageReceived(testing::FloatNear(24.0f, 0.01f)));

        hal::Can::Message data;
        data.resize(8, 0);
        data[0] = 0;
        data[1] = 240; // voltage wire = 240 → physical = 240 / focVoltageScale(10) = 24.0 V
        data[2] = 0;
        data[3] = 0;
        data[4] = 0;
        data[5] = 50; // iq wire = 50 → physical = 50 / focCurrentScale(10) = 5.0 A
        data[6] = 0;
        data[7] = 30; // id wire = 30 → physical = 30 / focCurrentScale(10) = 3.0 A

        auto canId = hal::Can::Id::Create29BitId(
            services::MakeCanId(services::CanPriority::telemetry,
                can::focMotorCategoryId,
                can::focTelemetryElectricalResponseId,
                1));
        receiveCallback(canId, data);
    }

    TEST_F(TestCanCommandClient, motor_type_response_received_does_not_call_any_observer)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));

        hal::Can::Message data;
        data.push_back(static_cast<uint8_t>(can::FocMotorMode::speed));

        auto canId = hal::Can::Id::Create29BitId(
            MakeCanId(services::CanPriority::response,
                can::focMotorCategoryId,
                can::focMotorTypeResponseId,
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
            services::MakeCanId(services::CanPriority::response,
                can::focMotorCategoryId,
                can::focElectricalParamsResponseId,
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
            services::MakeCanId(services::CanPriority::response,
                can::focMotorCategoryId,
                can::focMechanicalParamsResponseId,
                1));
        receiveCallback(canId, data);
        // StrictMock: no observer method should be called — OnMechanicalParamsResponse is a no-op
    }

    // ---------- SelectControlModeResponse forwarding ----------

    TEST_F(TestCanCommandClient, select_control_mode_response_notifies_control_mode_acknowledged)
    {
        EXPECT_CALL(observer, OnConnectionChanged(true));
        EXPECT_CALL(observer, OnControlModeAcknowledged(can::FocMotorMode::speed));

        hal::Can::Message data;
        data.resize(1, 0);
        data[0] = static_cast<uint8_t>(can::FocMotorMode::speed);

        auto canId = hal::Can::Id::Create29BitId(
            services::MakeCanId(services::CanPriority::response,
                can::focMotorCategoryId,
                can::focSelectControlModeResponseId,
                1));
        receiveCallback(canId, data);
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

        client.SendSetTorqueSetpoint(5.0f); // 5.0 A * focCurrentScale(10) = 50 = 0x0032

        ASSERT_EQ(capturedData.size(), 3u);
        EXPECT_EQ(capturedData[1], 0x00u); // high byte of 50
        EXPECT_EQ(capturedData[2], 0x32u); // low  byte of 50
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

        client.SendSetSpeedSetpoint(100000.0f); // 100000 * focSpeedScale(1) = 100000 > INT16_MAX → clamped to 32767 = 0x7FFF

        ASSERT_EQ(capturedData.size(), 3u);
        EXPECT_EQ(capturedData[1], 0x7Fu); // high byte of INT16_MAX
        EXPECT_EQ(capturedData[2], 0xFFu); // low  byte of INT16_MAX
    }

    TEST_F(TestCanCommandClient, send_speed_setpoint_overrange_negative_wire_value_clamped_to_int16_min)
    {
        hal::Can::Message capturedData;
        EXPECT_CALL(adapter, SendData(_, _, _))
            .WillOnce(Invoke([&capturedData](hal::Can::Id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                {
                    capturedData = msg;
                    cb(true);
                }));

        client.SendSetSpeedSetpoint(-100000.0f); // -100000 * focSpeedScale(1) = -100000 < INT16_MIN → clamped to -32768 = 0x8000

        ASSERT_EQ(capturedData.size(), 3u);
        EXPECT_EQ(capturedData[1], 0x80u); // high byte of INT16_MIN
        EXPECT_EQ(capturedData[2], 0x00u); // low  byte of INT16_MIN
    }

    TEST_F(TestCanCommandClient, send_position_setpoint_encodes_with_correct_scale)
    {
        hal::Can::Message capturedData;
        EXPECT_CALL(adapter, SendData(_, _, _))
            .WillOnce(Invoke([&capturedData](hal::Can::Id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                {
                    capturedData = msg;
                    cb(true);
                }));

        client.SendSetPositionSetpoint(0.1f); // 0.1 * focPositionScale(100) = 10 = 0x000A

        ASSERT_EQ(capturedData.size(), 3u);
        EXPECT_EQ(capturedData[1], 0x00u); // high byte of 10
        EXPECT_EQ(capturedData[2], 0x0Au); // low  byte of 10
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
        client.SendSetControlMode(can::FocMotorMode::speed);
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
