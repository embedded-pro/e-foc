#include "source/services/can_protocol/CanFrameCodec.hpp"
#include "source/services/can_protocol/CanProtocolDefinitions.hpp"
#include "source/tool/can_commander/logic/test/CanBusAdapterMock.hpp"
#include "source/tool/can_commander/logic/test/CanCommandClientObserverMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace tool;
    using namespace services;
    using testing::_;
    using testing::Eq;
    using testing::NiceMock;

    class TestCanCommandClient : public testing::Test
    {
    public:
        TestCanCommandClient()
            : client(adapter)
            , observer(client)
        {}

        void SimulateRx(uint32_t id, const CanFrame& data)
        {
            adapter.NotifyObservers([id, &data](auto& obs)
                {
                    obs.OnFrameReceived(id, data);
                });
        }

        NiceMock<CanBusAdapterMock> adapter;
        CanCommandClient client;
        NiceMock<CanCommandClientObserverMock> observer;
    };

    // ---------- SendStartMotor ----------

    TEST_F(TestCanCommandClient, send_start_motor_sends_correct_frame)
    {
        uint32_t expectedId = MakeCanId(CanPriority::command, CanCategory::motorControl, CanMessageType::startMotor, 1);

        EXPECT_CALL(adapter, Send(Eq(expectedId), _)).WillOnce([](uint32_t, const CanFrame& data)
            {
                EXPECT_EQ(data.size(), 1u);
                EXPECT_EQ(data[0], 1);
                return true;
            });

        client.SendStartMotor();
    }

    TEST_F(TestCanCommandClient, send_start_motor_sets_busy)
    {
        EXPECT_CALL(observer, OnBusyChanged(true));
        client.SendStartMotor();
        EXPECT_TRUE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_start_motor_logs_tx_frame)
    {
        EXPECT_CALL(observer, OnFrameLog(true, _, _));
        client.SendStartMotor();
    }

    // ---------- SendStopMotor ----------

    TEST_F(TestCanCommandClient, send_stop_motor_sends_correct_frame)
    {
        uint32_t expectedId = MakeCanId(CanPriority::command, CanCategory::motorControl, CanMessageType::stopMotor, 1);

        EXPECT_CALL(adapter, Send(Eq(expectedId), _)).WillOnce([](uint32_t, const CanFrame& data)
            {
                EXPECT_EQ(data.size(), 1u);
                return true;
            });

        client.SendStopMotor();
    }

    // ---------- SendEmergencyStop ----------

    TEST_F(TestCanCommandClient, send_emergency_stop_uses_emergency_priority)
    {
        uint32_t expectedId = MakeCanId(CanPriority::emergency, CanCategory::motorControl, CanMessageType::emergencyStop, 1);

        EXPECT_CALL(adapter, Send(Eq(expectedId), _)).WillOnce([](uint32_t, const CanFrame& data)
            {
                EXPECT_EQ(data.size(), 1u);
                EXPECT_EQ(data[0], 0);
                return true;
            });

        client.SendEmergencyStop();
    }

    // ---------- SendSetControlMode ----------

    TEST_F(TestCanCommandClient, send_set_control_mode_encodes_mode_byte)
    {
        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t, const CanFrame& data)
            {
                EXPECT_EQ(data.size(), 2u);
                EXPECT_EQ(data[1], static_cast<uint8_t>(CanControlMode::speed));
                return true;
            });

        client.SendSetControlMode(CanControlMode::speed);
    }

    // ---------- SendSetTorqueSetpoint ----------

    TEST_F(TestCanCommandClient, send_set_torque_setpoint_encodes_currents)
    {
        float idCurrent = 1.5f;
        float iqCurrent = -2.0f;

        EXPECT_CALL(adapter, Send(_, _)).WillOnce([idCurrent, iqCurrent](uint32_t, const CanFrame& data)
            {
                EXPECT_EQ(data.size(), 5u);

                int16_t idFixed = CanFrameCodec::FloatToFixed16(idCurrent, canCurrentScale);
                int16_t iqFixed = CanFrameCodec::FloatToFixed16(iqCurrent, canCurrentScale);

                EXPECT_EQ(data[1], static_cast<uint8_t>(idFixed >> 8));
                EXPECT_EQ(data[2], static_cast<uint8_t>(idFixed & 0xFF));
                EXPECT_EQ(data[3], static_cast<uint8_t>(iqFixed >> 8));
                EXPECT_EQ(data[4], static_cast<uint8_t>(iqFixed & 0xFF));
                return true;
            });

        client.SendSetTorqueSetpoint(idCurrent, iqCurrent);
    }

    // ---------- SendSetSpeedSetpoint ----------

    TEST_F(TestCanCommandClient, send_set_speed_setpoint_encodes_32bit)
    {
        float speed = 10.5f;

        EXPECT_CALL(adapter, Send(_, _)).WillOnce([speed](uint32_t, const CanFrame& data)
            {
                EXPECT_EQ(data.size(), 5u);

                int32_t fixed = CanFrameCodec::FloatToFixed32(speed, canSpeedScale);
                EXPECT_EQ(data[1], static_cast<uint8_t>((fixed >> 24) & 0xFF));
                EXPECT_EQ(data[2], static_cast<uint8_t>((fixed >> 16) & 0xFF));
                EXPECT_EQ(data[3], static_cast<uint8_t>((fixed >> 8) & 0xFF));
                EXPECT_EQ(data[4], static_cast<uint8_t>(fixed & 0xFF));
                return true;
            });

        client.SendSetSpeedSetpoint(speed);
    }

    // ---------- SendSetPositionSetpoint ----------

    TEST_F(TestCanCommandClient, send_set_position_setpoint_encodes_32bit)
    {
        float pos = 3.14f;

        EXPECT_CALL(adapter, Send(_, _)).WillOnce([pos](uint32_t, const CanFrame& data)
            {
                EXPECT_EQ(data.size(), 5u);

                int32_t fixed = CanFrameCodec::FloatToFixed32(pos, canPositionScale);
                EXPECT_EQ(data[1], static_cast<uint8_t>((fixed >> 24) & 0xFF));
                EXPECT_EQ(data[2], static_cast<uint8_t>((fixed >> 16) & 0xFF));
                EXPECT_EQ(data[3], static_cast<uint8_t>((fixed >> 8) & 0xFF));
                EXPECT_EQ(data[4], static_cast<uint8_t>(fixed & 0xFF));
                return true;
            });

        client.SendSetPositionSetpoint(pos);
    }

    // ---------- PID commands ----------

    TEST_F(TestCanCommandClient, send_set_current_id_pid_encodes_three_gains)
    {
        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t id, const CanFrame& data)
            {
                EXPECT_EQ(ExtractCanCategory(id), CanCategory::pidTuning);
                EXPECT_EQ(ExtractCanMessageType(id), CanMessageType::setCurrentIdPid);
                EXPECT_EQ(data.size(), 7u);
                return true;
            });

        client.SendSetCurrentIdPid(1.0f, 0.5f, 0.1f);
    }

    TEST_F(TestCanCommandClient, send_set_current_iq_pid_sends_correct_type)
    {
        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t id, const CanFrame&)
            {
                EXPECT_EQ(ExtractCanMessageType(id), CanMessageType::setCurrentIqPid);
                return true;
            });

        client.SendSetCurrentIqPid(1.0f, 0.5f, 0.1f);
    }

    TEST_F(TestCanCommandClient, send_set_speed_pid_sends_correct_type)
    {
        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t id, const CanFrame&)
            {
                EXPECT_EQ(ExtractCanMessageType(id), CanMessageType::setSpeedPid);
                return true;
            });

        client.SendSetSpeedPid(1.0f, 0.5f, 0.1f);
    }

    TEST_F(TestCanCommandClient, send_set_position_pid_sends_correct_type)
    {
        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t id, const CanFrame&)
            {
                EXPECT_EQ(ExtractCanMessageType(id), CanMessageType::setPositionPid);
                return true;
            });

        client.SendSetPositionPid(1.0f, 0.5f, 0.1f);
    }

    // ---------- Motor parameters ----------

    TEST_F(TestCanCommandClient, send_set_pole_pairs_encodes_byte)
    {
        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t id, const CanFrame& data)
            {
                EXPECT_EQ(ExtractCanMessageType(id), CanMessageType::setPolePairs);
                EXPECT_EQ(data.size(), 2u);
                EXPECT_EQ(data[1], 7);
                return true;
            });

        client.SendSetPolePairs(7);
    }

    TEST_F(TestCanCommandClient, send_set_resistance_encodes_32bit)
    {
        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t id, const CanFrame& data)
            {
                EXPECT_EQ(ExtractCanMessageType(id), CanMessageType::setResistance);
                EXPECT_EQ(data.size(), 5u);
                return true;
            });

        client.SendSetResistance(0.5f);
    }

    TEST_F(TestCanCommandClient, send_set_inductance_encodes_32bit)
    {
        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t id, const CanFrame& data)
            {
                EXPECT_EQ(ExtractCanMessageType(id), CanMessageType::setInductance);
                EXPECT_EQ(data.size(), 5u);
                return true;
            });

        client.SendSetInductance(0.001f);
    }

    TEST_F(TestCanCommandClient, send_set_flux_linkage_encodes_32bit)
    {
        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t id, const CanFrame& data)
            {
                EXPECT_EQ(ExtractCanMessageType(id), CanMessageType::setFluxLinkage);
                EXPECT_EQ(data.size(), 5u);
                return true;
            });

        client.SendSetFluxLinkage(0.01f);
    }

    // ---------- System parameters ----------

    TEST_F(TestCanCommandClient, send_set_supply_voltage_encodes_16bit)
    {
        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t id, const CanFrame& data)
            {
                EXPECT_EQ(ExtractCanMessageType(id), CanMessageType::setSupplyVoltage);
                EXPECT_EQ(data.size(), 3u);
                return true;
            });

        client.SendSetSupplyVoltage(24.0f);
    }

    TEST_F(TestCanCommandClient, send_set_max_current_encodes_16bit)
    {
        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t id, const CanFrame& data)
            {
                EXPECT_EQ(ExtractCanMessageType(id), CanMessageType::setMaxCurrent);
                EXPECT_EQ(data.size(), 3u);
                return true;
            });

        client.SendSetMaxCurrent(10.0f);
    }

    // ---------- Heartbeat and RequestStatus ----------

    TEST_F(TestCanCommandClient, send_heartbeat_uses_heartbeat_priority)
    {
        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t id, const CanFrame& data)
            {
                EXPECT_EQ(ExtractCanPriority(id), CanPriority::heartbeat);
                EXPECT_EQ(ExtractCanCategory(id), CanCategory::system);
                EXPECT_EQ(data.size(), 1u);
                EXPECT_EQ(data[0], canProtocolVersion);
                return true;
            });

        client.SendHeartbeat();
    }

    TEST_F(TestCanCommandClient, send_heartbeat_does_not_set_busy)
    {
        EXPECT_CALL(observer, OnBusyChanged(_)).Times(0);
        client.SendHeartbeat();
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, send_request_status_sends_empty_payload)
    {
        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t id, const CanFrame& data)
            {
                EXPECT_EQ(ExtractCanMessageType(id), CanMessageType::requestStatus);
                EXPECT_EQ(data.size(), 0u);
                return true;
            });

        client.SendRequestStatus();
    }

    // ---------- Node ID ----------

    TEST_F(TestCanCommandClient, set_node_id_is_reflected_in_sent_frames)
    {
        client.SetNodeId(42);
        EXPECT_EQ(client.NodeId(), 42);

        EXPECT_CALL(adapter, Send(_, _)).WillOnce([](uint32_t id, const CanFrame&)
            {
                EXPECT_EQ(ExtractCanNodeId(id), 42);
                return true;
            });

        client.SendStartMotor();
    }

    // ---------- Sequence counter ----------

    TEST_F(TestCanCommandClient, sequence_counter_increments)
    {
        std::vector<uint8_t> sequences;

        EXPECT_CALL(adapter, Send(_, _)).Times(3).WillRepeatedly([&sequences](uint32_t, const CanFrame& data)
            {
                sequences.push_back(data[0]);
                return true;
            });

        client.SendStartMotor();
        client.SendStopMotor();
        client.SendStartMotor();

        EXPECT_EQ(sequences.size(), 3u);
        EXPECT_EQ(sequences[0], 1);
        EXPECT_EQ(sequences[1], 2);
        EXPECT_EQ(sequences[2], 3);
    }

    // ---------- Busy state ----------

    TEST_F(TestCanCommandClient, initially_not_busy)
    {
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, busy_cleared_on_ack)
    {
        client.SendStartMotor();
        EXPECT_TRUE(client.IsBusy());

        CanFrame ackData;
        ackData.push_back(static_cast<uint8_t>(CanCategory::motorControl));
        ackData.push_back(static_cast<uint8_t>(CanMessageType::startMotor));
        ackData.push_back(static_cast<uint8_t>(CanAckStatus::success));

        uint32_t ackId = MakeCanId(CanPriority::command, CanCategory::system, CanMessageType::commandAck, 1);

        EXPECT_CALL(observer, OnBusyChanged(false));
        EXPECT_CALL(observer, OnCommandAckReceived(CanCategory::motorControl, CanMessageType::startMotor, CanAckStatus::success));

        SimulateRx(ackId, ackData);
        EXPECT_FALSE(client.IsBusy());
    }

    TEST_F(TestCanCommandClient, busy_cleared_on_timeout)
    {
        client.SendStartMotor();

        EXPECT_CALL(observer, OnBusyChanged(false));
        EXPECT_CALL(observer, OnCommandTimeout());

        client.HandleTimeout();
        EXPECT_FALSE(client.IsBusy());
    }

    // ---------- Telemetry: motor status ----------

    TEST_F(TestCanCommandClient, motor_status_telemetry_decoded)
    {
        CanFrame data;
        data.push_back(static_cast<uint8_t>(CanMotorState::running));
        data.push_back(static_cast<uint8_t>(CanControlMode::torque));
        data.push_back(static_cast<uint8_t>(CanFaultCode::none));

        uint32_t id = MakeCanId(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::motorStatus, 1);

        EXPECT_CALL(observer, OnMotorStatusReceived(CanMotorState::running, CanControlMode::torque, CanFaultCode::none));
        EXPECT_CALL(observer, OnControlModeReceived(CanControlMode::torque));

        SimulateRx(id, data);
    }

    // ---------- Telemetry: current measurement ----------

    TEST_F(TestCanCommandClient, current_measurement_decoded)
    {
        float expectedId = 1.5f;
        float expectedIq = -2.0f;

        int16_t idRaw = CanFrameCodec::FloatToFixed16(expectedId, canCurrentScale);
        int16_t iqRaw = CanFrameCodec::FloatToFixed16(expectedIq, canCurrentScale);

        CanFrame data;
        data.push_back(static_cast<uint8_t>(idRaw >> 8));
        data.push_back(static_cast<uint8_t>(idRaw & 0xFF));
        data.push_back(static_cast<uint8_t>(iqRaw >> 8));
        data.push_back(static_cast<uint8_t>(iqRaw & 0xFF));

        uint32_t id = MakeCanId(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::currentMeasurement, 1);

        EXPECT_CALL(observer, OnCurrentMeasurementReceived(testing::FloatEq(expectedId), testing::FloatEq(expectedIq)));

        SimulateRx(id, data);
    }

    // ---------- Telemetry: speed/position ----------

    TEST_F(TestCanCommandClient, speed_position_decoded)
    {
        float expectedSpeed = 5.0f;
        float expectedPos = 1.0f;

        int16_t speedRaw = CanFrameCodec::FloatToFixed16(expectedSpeed, canSpeedScale);
        int16_t posRaw = CanFrameCodec::FloatToFixed16(expectedPos, canPositionScale);

        CanFrame data;
        data.push_back(static_cast<uint8_t>(speedRaw >> 8));
        data.push_back(static_cast<uint8_t>(speedRaw & 0xFF));
        data.push_back(static_cast<uint8_t>(posRaw >> 8));
        data.push_back(static_cast<uint8_t>(posRaw & 0xFF));

        uint32_t id = MakeCanId(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::speedPosition, 1);

        EXPECT_CALL(observer, OnSpeedPositionReceived(testing::FloatEq(expectedSpeed), testing::FloatEq(expectedPos)));

        SimulateRx(id, data);
    }

    // ---------- Telemetry: bus voltage ----------

    TEST_F(TestCanCommandClient, bus_voltage_decoded)
    {
        float expectedVoltage = 24.0f;
        int16_t raw = CanFrameCodec::FloatToFixed16(expectedVoltage, canVoltageScale);

        CanFrame data;
        data.push_back(static_cast<uint8_t>(raw >> 8));
        data.push_back(static_cast<uint8_t>(raw & 0xFF));

        uint32_t id = MakeCanId(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::busVoltage, 1);

        EXPECT_CALL(observer, OnBusVoltageReceived(testing::FloatEq(expectedVoltage)));

        SimulateRx(id, data);
    }

    // ---------- Telemetry: fault event ----------

    TEST_F(TestCanCommandClient, fault_event_decoded)
    {
        CanFrame data;
        data.push_back(static_cast<uint8_t>(CanFaultCode::overCurrent));

        uint32_t id = MakeCanId(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::faultEvent, 1);

        EXPECT_CALL(observer, OnFaultEventReceived(CanFaultCode::overCurrent));

        SimulateRx(id, data);
    }

    // ---------- System: heartbeat received ----------

    TEST_F(TestCanCommandClient, heartbeat_received_decoded)
    {
        CanFrame data;
        data.push_back(canProtocolVersion);

        uint32_t id = MakeCanId(CanPriority::heartbeat, CanCategory::system, CanMessageType::heartbeat, 1);

        EXPECT_CALL(observer, OnHeartbeatReceived(canProtocolVersion));

        SimulateRx(id, data);
    }

    // ---------- Rx frame log ----------

    TEST_F(TestCanCommandClient, received_frames_are_logged)
    {
        CanFrame data;
        data.push_back(0x42);

        uint32_t id = MakeCanId(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::busVoltage, 1);

        EXPECT_CALL(observer, OnFrameLog(false, id, _));

        SimulateRx(id, data);
    }

    // ---------- Adapter event relay ----------

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

    // ---------- Short data ignored ----------

    TEST_F(TestCanCommandClient, motor_status_ignores_short_data)
    {
        CanFrame data;
        data.push_back(0x01);
        data.push_back(0x02);

        uint32_t id = MakeCanId(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::motorStatus, 1);

        EXPECT_CALL(observer, OnMotorStatusReceived(_, _, _)).Times(0);

        SimulateRx(id, data);
    }

    TEST_F(TestCanCommandClient, current_measurement_ignores_short_data)
    {
        CanFrame data;
        data.push_back(0x01);

        uint32_t id = MakeCanId(CanPriority::telemetry, CanCategory::telemetry, CanMessageType::currentMeasurement, 1);

        EXPECT_CALL(observer, OnCurrentMeasurementReceived(_, _)).Times(0);

        SimulateRx(id, data);
    }

    TEST_F(TestCanCommandClient, command_ack_ignores_short_data)
    {
        CanFrame data;
        data.push_back(0x01);
        data.push_back(0x02);

        uint32_t id = MakeCanId(CanPriority::command, CanCategory::system, CanMessageType::commandAck, 1);

        EXPECT_CALL(observer, OnCommandAckReceived(_, _, _)).Times(0);

        SimulateRx(id, data);
    }
}
