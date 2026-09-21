#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/CanSequenceSource.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/can/FocMotorCategoryClient.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <set>
#include <vector>

namespace
{
    using namespace testing;

    struct AcknowledgerSpy
        : services::CanCommandAcknowledger
    {
        virtual ~AcknowledgerSpy() = default;

        struct Entry
        {
            uint8_t category{};
            uint8_t commandType{};
            services::CanAckStatus status{};
        };

        std::optional<Entry> last;

        void SendCommandAck(uint8_t category, uint8_t commandType, services::CanAckStatus status) override
        {
            last = Entry{ category, commandType, status };
        }

        void Reset()
        {
            last.reset();
        }
    };

    class ContractServerObserver
        : public can::FocMotorCategoryServerObserver
    {
    public:
        using can::FocMotorCategoryServerObserver::FocMotorCategoryServerObserver;

        MOCK_METHOD(void, OnStart, (const infra::Function<void(services::CanAckStatus)>&), (override));
        MOCK_METHOD(void, OnStop, (const infra::Function<void(services::CanAckStatus)>&), (override));
        MOCK_METHOD(void, OnClearFault, (const infra::Function<void(services::CanAckStatus)>&), (override));
        MOCK_METHOD(void, OnEmergencyStop, (const infra::Function<void(services::CanAckStatus)>&), (override));
        MOCK_METHOD(void, OnSelectControlMode, (can::FocMotorMode, (const infra::Function<void(can::FocMotorMode)>&)), (override));
        MOCK_METHOD(void, OnSetTorqueSetpoint, (foc::Ampere, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnSetSpeedSetpoint, (foc::RadiansPerSecond, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnSetPositionSetpoint, (foc::Radians, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnSetPidCurrent, (float, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnSetPidSpeed, (float, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnSetPidPosition, (float, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnAlign, ((const infra::Function<void(services::CanAckStatus)>&)), (override));
        MOCK_METHOD(void, OnIdentifyElectrical, (const infra::Function<void()>&), (override));
        MOCK_METHOD(void, OnIdentifyMechanical, (const infra::Function<void()>&), (override));
        MOCK_METHOD(void, OnRequestTelemetry, (const infra::Function<void()>&), (override));
        MOCK_METHOD(void, OnSetEncoderResolution, (uint32_t, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnConfigureTelemetryRate, (uint32_t, (const infra::Function<void()>&)), (override));
    };

    class ContractClientObserver
        : public can::FocMotorCategoryClientObserver
    {
    public:
        using can::FocMotorCategoryClientObserver::FocMotorCategoryClientObserver;

        MOCK_METHOD(void, OnSelectControlModeResponse, (can::FocMotorMode), (override));
        MOCK_METHOD(void, OnCategoryError, (uint8_t, can::FocMotorCategoryError), (override));
        MOCK_METHOD(void, OnTelemetryStatus, (const hal::Can::Message&), (override));
        MOCK_METHOD(void, OnTelemetryElectrical, (const hal::Can::Message&), (override));
        MOCK_METHOD(void, OnContractVersionResponse, (uint8_t, uint8_t), (override));
    };

    struct SequenceSourceStub
        : services::CanSequenceSource
    {
        virtual ~SequenceSourceStub() = default;

        uint8_t PeekSequence(uint16_t) override
        {
            return sequence;
        }

        void CommitSequence(uint16_t, uint8_t, uint8_t) override
        {}

        uint8_t sequence{ 0 };
    };

    // The client and the server meet here, over one transport each, so a frame one side builds is the
    // frame the other side parses; every other test in this tree checks a side against its own mock
    class FocMotorWireContractTest
        : public Test
    {
    public:
        FocMotorWireContractTest()
        {
            EXPECT_CALL(clientCanMock, SendData(_, _, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([this](hal::Can::Id id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                    {
                        sentMessageType = services::ExtractCanMessageType(id.Get29BitId());
                        sentMessage = msg;
                        cb(true);
                    }));

            EXPECT_CALL(serverCanMock, SendData(_, _, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([this](hal::Can::Id id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                    {
                        respondedMessageType = services::ExtractCanMessageType(id.Get29BitId());
                        respondedMessage = msg;
                        cb(true);
                    }));

            server.SetAcknowledger(ackSpy);
        }

        void DeliverToServer()
        {
            server.HandleMessage(sentMessageType, sentMessage);
        }

        void DeliverToClient()
        {
            client.HandleMessage(respondedMessageType, respondedMessage);
        }

        hal::Can::Message Bytes(std::initializer_list<uint8_t> values) const
        {
            hal::Can::Message message;
            for (const auto value : values)
                message.push_back(value);
            return message;
        }

        StrictMock<hal::CanMock> clientCanMock;
        StrictMock<hal::CanMock> serverCanMock;
        services::CanFrameTransport clientTransport{ clientCanMock, 0 };
        services::CanFrameTransport serverTransport{ serverCanMock, 0 };
        SequenceSourceStub sequenceSource;

        can::FocMotorCategoryClient client{ clientTransport, sequenceSource };
        can::FocMotorCategoryServer server{ serverTransport };
        AcknowledgerSpy ackSpy;
        StrictMock<ContractServerObserver> serverObserver{ server };
        StrictMock<ContractClientObserver> clientObserver{ client };

        uint8_t sentMessageType{};
        hal::Can::Message sentMessage;
        uint8_t respondedMessageType{};
        hal::Can::Message respondedMessage;
    };
}

TEST_F(FocMotorWireContractTest, every_descriptor_fits_a_classic_can_frame_and_holds_a_unique_id)
{
    for (const auto& descriptor : can::wire::focMotorMessages)
    {
        EXPECT_LE(descriptor.PayloadSize(), 8u) << descriptor.name;
        EXPECT_LE(descriptor.id, 0x7Fu) << descriptor.name;
    }

    for (const auto& descriptor : can::wire::focMotorResponses)
    {
        EXPECT_LE(descriptor.PayloadSize(), 8u) << descriptor.name;
        EXPECT_GE(descriptor.id, 0x80u) << descriptor.name;
    }

    std::vector<uint8_t> ids;
    for (const auto& descriptor : can::wire::focMotorMessages)
        ids.push_back(descriptor.id);
    for (const auto& descriptor : can::wire::focMotorResponses)
        ids.push_back(descriptor.id);

    const auto unique = std::set<uint8_t>{ ids.begin(), ids.end() };
    EXPECT_EQ(ids.size(), unique.size());
}

TEST_F(FocMotorWireContractTest, a_bandwidth_command_is_the_documented_three_bytes)
{
    client.SendSetCurrentBandwidth(1, 250.0f);

    EXPECT_EQ(can::focSetPidCurrentId, sentMessageType);
    EXPECT_EQ(Bytes({ 0x00, 0x00, 0xFA }), sentMessage);
}

TEST_F(FocMotorWireContractTest, a_torque_setpoint_is_the_documented_three_bytes_at_the_documented_scale)
{
    client.SendSetTorqueSetpoint(1, foc::Ampere{ 1.5f });

    EXPECT_EQ(can::focSetTorqueSetpointId, sentMessageType);
    EXPECT_EQ(Bytes({ 0x00, 0x00, 0x0F }), sentMessage);
}

TEST_F(FocMotorWireContractTest, a_position_setpoint_is_the_documented_three_bytes_at_the_documented_scale)
{
    client.SendSetPositionSetpoint(1, foc::Radians{ 3.14f });

    EXPECT_EQ(can::focSetPositionSetpointId, sentMessageType);
    EXPECT_EQ(Bytes({ 0x00, 0x01, 0x3A }), sentMessage);
}

TEST_F(FocMotorWireContractTest, a_commandless_payload_still_carries_the_sequence_byte)
{
    client.SendStart(1);

    EXPECT_EQ(can::focStartId, sentMessageType);
    EXPECT_EQ(Bytes({ 0x00 }), sentMessage);
}

TEST_F(FocMotorWireContractTest, a_mode_command_is_the_documented_two_bytes)
{
    client.SendSelectControlMode(1, can::FocMotorMode::position);

    EXPECT_EQ(can::focSelectControlModeId, sentMessageType);
    EXPECT_EQ(Bytes({ 0x00, 0x02 }), sentMessage);
}

TEST_F(FocMotorWireContractTest, a_telemetry_status_frame_is_the_documented_six_bytes)
{
    server.BroadcastTelemetryStatus(can::FocMotorState::partialCalibration, can::FocFaultCode::overVoltage);

    EXPECT_EQ(can::focTelemetryStatusResponseId, respondedMessageType);
    EXPECT_EQ(Bytes({ 0x04, 0x02, 0x00, 0x00, 0x00, 0x00 }), respondedMessage);
}

TEST_F(FocMotorWireContractTest, every_command_the_client_sends_reaches_the_server_with_its_value_intact)
{
    EXPECT_CALL(serverObserver, OnSetPidCurrent(FloatEq(250.0f), _));
    client.SendSetCurrentBandwidth(1, 250.0f);
    DeliverToServer();

    EXPECT_CALL(serverObserver, OnSetTorqueSetpoint(foc::Ampere{ 1.5f }, _));
    client.SendSetTorqueSetpoint(1, foc::Ampere{ 1.5f });
    DeliverToServer();

    EXPECT_CALL(serverObserver, OnSetSpeedSetpoint(foc::RadiansPerSecond{ 300.0f }, _));
    client.SendSetSpeedSetpoint(1, foc::RadiansPerSecond{ 300.0f });
    DeliverToServer();

    EXPECT_CALL(serverObserver, OnSelectControlMode(can::FocMotorMode::speed, _));
    client.SendSelectControlMode(1, can::FocMotorMode::speed);
    DeliverToServer();

    EXPECT_CALL(serverObserver, OnStart(_));
    client.SendStart(1);
    DeliverToServer();

    EXPECT_CALL(serverObserver, OnEmergencyStop(_));
    client.SendEmergencyStop(1);
    DeliverToServer();
}

TEST_F(FocMotorWireContractTest, a_select_control_mode_response_reaches_the_client)
{
    EXPECT_CALL(clientObserver, OnSelectControlModeResponse(can::FocMotorMode::position));

    server.SendSelectControlModeResponse(can::FocMotorMode::position);
    DeliverToClient();
}

TEST_F(FocMotorWireContractTest, the_contract_version_round_trips_and_both_ends_agree)
{
    client.SendQueryContractVersion(1);
    DeliverToServer();

    EXPECT_EQ(can::focContractVersionResponseId, respondedMessageType);
    EXPECT_EQ(Bytes({ can::wire::focContractVersionMajor, can::wire::focContractVersionMinor }), respondedMessage);

    EXPECT_CALL(clientObserver, OnContractVersionResponse(can::wire::focContractVersionMajor, can::wire::focContractVersionMinor));
    DeliverToClient();
}

TEST_F(FocMotorWireContractTest, a_can_lite_shaped_gains_frame_is_rejected_rather_than_read_as_a_bandwidth)
{
    // The pinned can-lite example encodes kp/ki/kd here; reading its kp as a bandwidth is the failure
    // this exactness check exists to prevent
    const auto gainsFrame = Bytes({ 0x00, 0x00, 0x64, 0x00, 0x0A, 0x00, 0x01 });

    server.HandleMessage(can::focSetPidCurrentId, gainsFrame);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(can::focSetPidCurrentId, ackSpy.last->commandType);
    EXPECT_EQ(services::CanAckStatus::invalidPayload, ackSpy.last->status);
}

TEST_F(FocMotorWireContractTest, every_command_is_rejected_one_byte_short_and_one_byte_long)
{
    for (const auto& descriptor : can::wire::focMotorMessages)
    {
        const auto size = descriptor.PayloadSize();

        if (size > 0)
        {
            hal::Can::Message tooShort;
            tooShort.resize(static_cast<std::size_t>(size - 1), 0);
            ackSpy.Reset();
            server.HandleMessage(descriptor.id, tooShort);
            ASSERT_TRUE(ackSpy.last.has_value()) << descriptor.name;
            EXPECT_EQ(services::CanAckStatus::invalidPayload, ackSpy.last->status) << descriptor.name;
        }

        hal::Can::Message tooLong;
        tooLong.resize(static_cast<std::size_t>(size + 1), 0);
        ackSpy.Reset();
        server.HandleMessage(descriptor.id, tooLong);
        ASSERT_TRUE(ackSpy.last.has_value()) << descriptor.name;
        EXPECT_EQ(services::CanAckStatus::invalidPayload, ackSpy.last->status) << descriptor.name;
    }
}
