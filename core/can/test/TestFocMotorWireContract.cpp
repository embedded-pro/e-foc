#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/CanSequenceSource.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/can/FocMotorCategoryClient.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <set>
#include <vector>

namespace
{
    using namespace testing;

    struct AcknowledgerSpy
        : services::CanCommandAcknowledger
    {
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
        MOCK_METHOD(void, OnSelectAlgorithm, (can::FocControlLoop, uint8_t, (const infra::Function<void(const can::FocAlgorithms&)>&)), (override));
        MOCK_METHOD(void, OnQueryAlgorithms, ((const infra::Function<void(const can::FocAlgorithms&)>&)), (override));
        MOCK_METHOD(void, OnSetParameter, (can::FocParameter, float, (const infra::Function<void(can::FocParameterValue)>&)), (override));
        MOCK_METHOD(void, OnQueryParameter, (can::FocParameter, (const infra::Function<void(can::FocParameterValue)>&)), (override));
    };

    class ContractClientObserver
        : public can::FocMotorCategoryClientObserver
    {
    public:
        using can::FocMotorCategoryClientObserver::FocMotorCategoryClientObserver;

        MOCK_METHOD(void, OnSelectControlModeResponse, (uint16_t, can::FocMotorMode), (override));
        MOCK_METHOD(void, OnCategoryError, (uint16_t, uint8_t, can::FocMotorCategoryError), (override));
        MOCK_METHOD(void, OnTelemetryStatus, (uint16_t, const hal::Can::Message&), (override));
        MOCK_METHOD(void, OnTelemetryElectrical, (uint16_t, const hal::Can::Message&), (override));
        MOCK_METHOD(void, OnContractVersionResponse, (uint16_t, uint8_t, uint8_t), (override));
        MOCK_METHOD(void, OnElectricalParamsResponse, (uint16_t, foc::Ohm, foc::MilliHenry, uint8_t), (override));
        MOCK_METHOD(void, OnMechanicalParamsResponse, (uint16_t, foc::NewtonMeterSecondPerRadian, foc::NewtonMeterSecondSquared), (override));
        MOCK_METHOD(void, OnAlgorithmsResponse, (uint16_t, const can::FocAlgorithms&), (override));
        MOCK_METHOD(void, OnParameterResponse, (uint16_t, can::FocParameter, can::FocParameterValue), (override));
    };

    struct FrameOriginStub
        : can::CanFrameOrigin
    {
        uint16_t SourceNodeId() const override
        {
            return node;
        }

        uint16_t node{ 1 };
    };

    struct SequenceSourceStub
        : services::CanSequenceSource
    {
        uint8_t PeekSequence(uint16_t) override
        {
            return sequence;
        }

        void CommitSequence(uint16_t, uint8_t, uint8_t) override
        {}

        uint8_t sequence{ 0 };
    };

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

        FrameOriginStub origin;
        can::FocMotorCategoryClient client{ clientTransport, sequenceSource, origin };
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
    for (const auto& descriptor : can::focMotorMessages)
    {
        EXPECT_LE(descriptor.PayloadSize(), 8u) << descriptor.name;
        EXPECT_LE(descriptor.id, 0x7Fu) << descriptor.name;
    }

    for (const auto& descriptor : can::focMotorResponses)
    {
        EXPECT_LE(descriptor.PayloadSize(), 8u) << descriptor.name;
        EXPECT_GE(descriptor.id, 0x80u) << descriptor.name;
    }

    std::vector<uint8_t> ids;
    for (const auto& descriptor : can::focMotorMessages)
        ids.push_back(descriptor.id);
    for (const auto& descriptor : can::focMotorResponses)
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

TEST_F(FocMotorWireContractTest, an_algorithm_selection_is_the_documented_three_bytes)
{
    client.SendSelectAlgorithm(1, can::FocControlLoop::speed, static_cast<uint8_t>(can::FocSpeedAlgorithm::adrc));

    EXPECT_EQ(can::focSelectAlgorithmId, sentMessageType);
    EXPECT_EQ(Bytes({ 0x00, 0x01, 0x02 }), sentMessage);
}

TEST_F(FocMotorWireContractTest, a_parameter_write_carries_the_float_most_significant_byte_first)
{
    client.SendSetParameter(1, can::FocParameter::speedObserverBandwidthRatio, 8.0f);

    EXPECT_EQ(can::focSetParameterId, sentMessageType);
    EXPECT_EQ(Bytes({ 0x00, 0x13, 0x41, 0x00, 0x00, 0x00 }), sentMessage);
}

TEST_F(FocMotorWireContractTest, a_parameter_query_is_the_documented_two_bytes)
{
    client.SendQueryParameter(1, can::FocParameter::resistance);

    EXPECT_EQ(can::focQueryParameterId, sentMessageType);
    EXPECT_EQ(Bytes({ 0x00, 0x40 }), sentMessage);
}

TEST_F(FocMotorWireContractTest, a_parameter_response_flags_an_automatic_value)
{
    server.SendParameterResponse(can::FocParameter::currentBandwidth, can::FocParameterValue{ 1000.0f, true });

    EXPECT_EQ(can::focParameterResponseId, respondedMessageType);
    EXPECT_EQ(Bytes({ 0x00, 0x01, 0x44, 0x7A, 0x00, 0x00 }), respondedMessage);
}

TEST_F(FocMotorWireContractTest, an_algorithms_response_leads_with_the_active_mode)
{
    server.SendAlgorithmsResponse(can::FocAlgorithms{ can::FocMotorMode::position, can::FocCurrentAlgorithm::slidingMode,
        can::FocSpeedAlgorithm::twoDof, can::FocPositionAlgorithm::lqr });

    EXPECT_EQ(can::focAlgorithmsResponseId, respondedMessageType);
    EXPECT_EQ(Bytes({ 0x02, 0x03, 0x03, 0x02 }), respondedMessage);
}

TEST_F(FocMotorWireContractTest, a_telemetry_status_frame_is_the_documented_six_bytes)
{
    server.BroadcastTelemetryStatus(can::FocMotorState::partialCalibration, can::FocFaultCode::overVoltage,
        foc::RadiansPerSecond{ 0.0f }, foc::Radians{ 0.0f });

    EXPECT_EQ(can::focTelemetryStatusResponseId, respondedMessageType);
    EXPECT_EQ(Bytes({ 0x04, 0x02, 0x00, 0x00, 0x00, 0x00 }), respondedMessage);
}

TEST_F(FocMotorWireContractTest, a_telemetry_status_frame_carries_the_measured_speed_and_position)
{
    server.BroadcastTelemetryStatus(can::FocMotorState::running, can::FocFaultCode::none,
        foc::RadiansPerSecond{ 20.0f }, foc::Radians{ 1.5f });

    EXPECT_EQ(can::focTelemetryStatusResponseId, respondedMessageType);
    EXPECT_EQ(Bytes({ 0x01, 0x00, 0x00, 0x14, 0x00, 0x96 }), respondedMessage);
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

    EXPECT_CALL(serverObserver, OnSelectAlgorithm(can::FocControlLoop::position, static_cast<uint8_t>(can::FocPositionAlgorithm::twoDof), _));
    client.SendSelectAlgorithm(1, can::FocControlLoop::position, static_cast<uint8_t>(can::FocPositionAlgorithm::twoDof));
    DeliverToServer();

    EXPECT_CALL(serverObserver, OnSetParameter(can::FocParameter::inertia, FloatEq(7.06e-6f), _));
    client.SendSetParameter(1, can::FocParameter::inertia, 7.06e-6f);
    DeliverToServer();

    EXPECT_CALL(serverObserver, OnQueryParameter(can::FocParameter::canBitrate, _));
    client.SendQueryParameter(1, can::FocParameter::canBitrate);
    DeliverToServer();

    EXPECT_CALL(serverObserver, OnQueryAlgorithms(_));
    client.SendQueryAlgorithms(1);
    DeliverToServer();
}

TEST_F(FocMotorWireContractTest, a_parameter_answer_reaches_the_client_with_its_value_and_flag_intact)
{
    EXPECT_CALL(serverObserver, OnQueryParameter(can::FocParameter::fluxLinkage, _))
        .WillOnce(Invoke([](can::FocParameter, const infra::Function<void(can::FocParameterValue)>& onDone)
            {
                onDone(can::FocParameterValue{ 0.007f, true });
            }));
    client.SendQueryParameter(1, can::FocParameter::fluxLinkage);
    DeliverToServer();

    EXPECT_CALL(clientObserver, OnParameterResponse(1, can::FocParameter::fluxLinkage, can::FocParameterValue{ 0.007f, true }));
    DeliverToClient();
}

TEST_F(FocMotorWireContractTest, an_algorithms_answer_reaches_the_client_intact)
{
    const can::FocAlgorithms algorithms{ can::FocMotorMode::speed, can::FocCurrentAlgorithm::deadbeat,
        can::FocSpeedAlgorithm::lqi, can::FocPositionAlgorithm::cascadeP };

    EXPECT_CALL(serverObserver, OnSelectAlgorithm(can::FocControlLoop::speed, static_cast<uint8_t>(can::FocSpeedAlgorithm::lqi), _))
        .WillOnce(Invoke([&algorithms](can::FocControlLoop, uint8_t, const infra::Function<void(const can::FocAlgorithms&)>& onDone)
            {
                onDone(algorithms);
            }));
    client.SendSelectAlgorithm(1, can::FocControlLoop::speed, static_cast<uint8_t>(can::FocSpeedAlgorithm::lqi));
    DeliverToServer();

    EXPECT_CALL(clientObserver, OnAlgorithmsResponse(1, algorithms));
    DeliverToClient();
}

TEST_F(FocMotorWireContractTest, every_documented_parameter_value_round_trips_bit_exactly)
{
    for (const auto& parameter : can::focParameters)
    {
        for (const auto value : { parameter.minimum, parameter.maximum })
        {
            EXPECT_CALL(serverObserver, OnSetParameter(parameter.id, value, _));
            client.SendSetParameter(1, parameter.id, value);
            DeliverToServer();
        }
    }
}

TEST_F(FocMotorWireContractTest, a_select_control_mode_response_reaches_the_client)
{
    EXPECT_CALL(clientObserver, OnSelectControlModeResponse(1, can::FocMotorMode::position));

    server.SendSelectControlModeResponse(can::FocMotorMode::position);
    DeliverToClient();
}

TEST_F(FocMotorWireContractTest, the_contract_version_round_trips_and_both_ends_agree)
{
    client.SendQueryContractVersion(1);
    DeliverToServer();

    EXPECT_EQ(can::focContractVersionResponseId, respondedMessageType);
    EXPECT_EQ(Bytes({ can::focContractVersionMajor, can::focContractVersionMinor }), respondedMessage);

    EXPECT_CALL(clientObserver, OnContractVersionResponse(1, can::focContractVersionMajor, can::focContractVersionMinor));
    DeliverToClient();
}

TEST_F(FocMotorWireContractTest, a_can_lite_shaped_gains_frame_is_rejected_rather_than_read_as_a_bandwidth)
{
    const auto gainsFrame = Bytes({ 0x00, 0x00, 0x64, 0x00, 0x0A, 0x00, 0x01 });

    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetPidCurrentId, gainsFrame));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorWireContractTest, every_command_is_rejected_one_byte_short_and_one_byte_long)
{
    for (const auto& descriptor : can::focMotorMessages)
    {
        const auto size = descriptor.PayloadSize();

        if (size > 0)
        {
            hal::Can::Message tooShort;
            tooShort.resize(static_cast<std::size_t>(size - 1), 0);
            EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(descriptor.id, tooShort)) << descriptor.name;
        }

        hal::Can::Message tooLong;
        tooLong.resize(static_cast<std::size_t>(size + 1), 0);
        EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(descriptor.id, tooLong)) << descriptor.name;
    }
}

TEST_F(FocMotorWireContractTest, sends_are_allowed_before_the_version_is_known)
{
    EXPECT_EQ(can::ContractCompatibility::unknown, client.Compatibility(1));
    EXPECT_TRUE(client.SendStart(1));
}

TEST_F(FocMotorWireContractTest, a_matching_major_marks_the_contract_compatible)
{
    EXPECT_CALL(clientObserver, OnContractVersionResponse(1, _, _));
    client.HandleMessage(can::focContractVersionResponseId,
        Bytes({ can::focContractVersionMajor, can::focContractVersionMinor }));

    EXPECT_EQ(can::ContractCompatibility::compatible, client.Compatibility(1));
    EXPECT_TRUE(client.SendStart(1));
}

TEST_F(FocMotorWireContractTest, an_incompatible_major_refuses_every_further_category_command)
{
    EXPECT_CALL(clientObserver, OnContractVersionResponse(1, _, _));
    client.HandleMessage(can::focContractVersionResponseId,
        Bytes({ static_cast<uint8_t>(can::focContractVersionMajor + 1), 0 }));

    EXPECT_EQ(can::ContractCompatibility::incompatible, client.Compatibility(1));

    EXPECT_FALSE(client.SendStart(1));
    EXPECT_FALSE(client.SendStop(1));
    EXPECT_FALSE(client.SendClearFault(1));
    EXPECT_FALSE(client.SendEmergencyStop(1));
    EXPECT_FALSE(client.SendSelectControlMode(1, can::FocMotorMode::speed));
    EXPECT_FALSE(client.SendSetTorqueSetpoint(1, foc::Ampere{ 1.0f }));
    EXPECT_FALSE(client.SendSetSpeedSetpoint(1, foc::RadiansPerSecond{ 1.0f }));
    EXPECT_FALSE(client.SendSetPositionSetpoint(1, foc::Radians{ 1.0f }));
    EXPECT_FALSE(client.SendSetCurrentBandwidth(1, 100.0f));
    EXPECT_FALSE(client.SendSetSpeedBandwidth(1, 100.0f));
    EXPECT_FALSE(client.SendSetPositionBandwidth(1, 100.0f));
    EXPECT_FALSE(client.SendAlign(1));
    EXPECT_FALSE(client.SendIdentifyElectrical(1));
    EXPECT_FALSE(client.SendIdentifyMechanical(1));
    EXPECT_FALSE(client.SendRequestTelemetry(1));
    EXPECT_FALSE(client.SendSetEncoderResolution(1, 4000));
    EXPECT_FALSE(client.SendSelectAlgorithm(1, can::FocControlLoop::current, 0));
    EXPECT_FALSE(client.SendQueryAlgorithms(1));
    EXPECT_FALSE(client.SendSetParameter(1, can::FocParameter::resistance, 0.36f));
    EXPECT_FALSE(client.SendQueryParameter(1, can::FocParameter::resistance));
}

TEST_F(FocMotorWireContractTest, an_incompatible_node_does_not_refuse_commands_to_other_nodes)
{
    EXPECT_CALL(clientObserver, OnContractVersionResponse(1, _, _));
    client.HandleMessage(can::focContractVersionResponseId,
        Bytes({ static_cast<uint8_t>(can::focContractVersionMajor + 1), 0 }));

    origin.node = 2;
    EXPECT_CALL(clientObserver, OnContractVersionResponse(2, _, _));
    client.HandleMessage(can::focContractVersionResponseId,
        Bytes({ can::focContractVersionMajor, can::focContractVersionMinor }));

    EXPECT_EQ(can::ContractCompatibility::incompatible, client.Compatibility(1));
    EXPECT_EQ(can::ContractCompatibility::compatible, client.Compatibility(2));
    EXPECT_EQ(can::ContractCompatibility::unknown, client.Compatibility(3));
    EXPECT_FALSE(client.SendEmergencyStop(1));
    EXPECT_TRUE(client.SendEmergencyStop(2));
    EXPECT_TRUE(client.SendEmergencyStop(3));
}

TEST_F(FocMotorWireContractTest, a_node_that_is_upgraded_reports_its_new_compatibility)
{
    EXPECT_CALL(clientObserver, OnContractVersionResponse(1, _, _)).Times(2);
    client.HandleMessage(can::focContractVersionResponseId,
        Bytes({ static_cast<uint8_t>(can::focContractVersionMajor + 1), 0 }));
    client.HandleMessage(can::focContractVersionResponseId,
        Bytes({ can::focContractVersionMajor, can::focContractVersionMinor }));

    EXPECT_EQ(can::ContractCompatibility::compatible, client.Compatibility(1));
    EXPECT_TRUE(client.SendStart(1));
}

TEST_F(FocMotorWireContractTest, compatibility_of_the_oldest_node_is_forgotten_beyond_the_tracked_limit)
{
    for (uint16_t node = 1; node <= can::FocMotorCategoryClient::maxTrackedNodes + 1; ++node)
    {
        origin.node = node;
        EXPECT_CALL(clientObserver, OnContractVersionResponse(node, _, _));
        client.HandleMessage(can::focContractVersionResponseId,
            Bytes({ static_cast<uint8_t>(can::focContractVersionMajor + 1), 0 }));
    }

    EXPECT_EQ(can::ContractCompatibility::unknown, client.Compatibility(1));
    EXPECT_EQ(can::ContractCompatibility::incompatible, client.Compatibility(2));
    EXPECT_EQ(can::ContractCompatibility::incompatible, client.Compatibility(can::FocMotorCategoryClient::maxTrackedNodes + 1));
}

TEST_F(FocMotorWireContractTest, the_version_query_itself_survives_an_incompatible_major)
{
    EXPECT_CALL(clientObserver, OnContractVersionResponse(1, _, _));
    client.HandleMessage(can::focContractVersionResponseId,
        Bytes({ static_cast<uint8_t>(can::focContractVersionMajor + 1), 0 }));

    EXPECT_TRUE(client.SendQueryContractVersion(1));
}

namespace
{
    struct GoldenVector
    {
        uint8_t id;
        std::vector<uint8_t> bytes;
    };

    const std::vector<GoldenVector>& GoldenVectors()
    {
        static const std::vector<GoldenVector> vectors{
            { can::focQueryMotorTypeId, { 0x00 } },
            { can::focStartId, { 0x00 } },
            { can::focStopId, { 0x00 } },
            { can::focSetPidCurrentId, { 0x00, 0x00, 0xFA } },
            { can::focSetPidSpeedId, { 0x00, 0x00, 0x32 } },
            { can::focSetPidPositionId, { 0x00, 0x00, 0x14 } },
            { can::focIdentifyElectricalId, { 0x00 } },
            { can::focIdentifyMechanicalId, { 0x00 } },
            { can::focRequestTelemetryId, { 0x00 } },
            { can::focSetEncoderResolutionId, { 0x00, 0x00, 0x00, 0x10, 0x00 } },
            { can::focAlignId, { 0x00 } },
            { can::focClearFaultId, { 0x00 } },
            { can::focEmergencyStopId, { 0x00 } },
            { can::focConfigureTelemetryRateId, { 0x00, 0x00, 0x00, 0x00, 0x64 } },
            { can::focSelectControlModeId, { 0x00, 0x02 } },
            { can::focSetTorqueSetpointId, { 0x00, 0x00, 0x0F } },
            { can::focSetSpeedSetpointId, { 0x00, 0x01, 0x2C } },
            { can::focSetPositionSetpointId, { 0x00, 0x01, 0x3A } },
            { can::focQueryContractVersionId, { 0x00 } },
            { can::focSelectAlgorithmId, { 0x00, 0x01, 0x02 } },
            { can::focQueryAlgorithmsId, { 0x00 } },
            { can::focSetParameterId, { 0x00, 0x13, 0x41, 0x00, 0x00, 0x00 } },
            { can::focQueryParameterId, { 0x00, 0x40 } },

            { can::focMotorTypeResponseId, { 0x01 } },
            { can::focElectricalParamsResponseId, { 0x01, 0xF4, 0x03, 0xE8, 0x07 } },
            { can::focMechanicalParamsResponseId, { 0x00, 0x00, 0x3A, 0x98, 0x00, 0x00, 0x1B, 0x94 } },
            { can::focTelemetryElectricalResponseId, { 0x00, 0xF0, 0x00, 0x96, 0x00, 0x0F, 0x00, 0x00 } },
            { can::focTelemetryStatusResponseId, { 0x04, 0x02, 0x00, 0x00, 0x00, 0x00 } },
            { can::focSelectControlModeResponseId, { 0x02 } },
            { can::focContractVersionResponseId, { can::focContractVersionMajor, can::focContractVersionMinor } },
            { can::focAlgorithmsResponseId, { 0x01, 0x00, 0x02, 0x00 } },
            { can::focParameterResponseId, { 0x40, 0x00, 0x3E, 0xB8, 0x51, 0xEC } },
            { services::canCategoryErrorResponseMessageTypeId, { can::focStartId, static_cast<uint8_t>(can::FocMotorCategoryError::busy) } },
        };
        return vectors;
    }

    std::optional<GoldenVector> GoldenVectorFor(uint8_t id)
    {
        for (const auto& vector : GoldenVectors())
        {
            if (vector.id == id)
                return vector;
        }

        return std::nullopt;
    }
}

TEST_F(FocMotorWireContractTest, every_message_in_the_contract_has_a_golden_vector)
{
    const auto check = [](const auto& messages)
    {
        for (const auto& descriptor : messages)
        {
            EXPECT_TRUE(GoldenVectorFor(descriptor.id).has_value()) << descriptor.name;
        }
    };

    check(can::focMotorMessages);
    check(can::focMotorResponses);

    EXPECT_EQ(can::focMotorMessages.size() + can::focMotorResponses.size(), GoldenVectors().size());
}

TEST_F(FocMotorWireContractTest, every_golden_vector_is_the_length_its_descriptor_states)
{
    for (const auto& vector : GoldenVectors())
    {
        const auto descriptor = can::FindDescriptor(vector.id);
        ASSERT_TRUE(descriptor.has_value());
        EXPECT_EQ(static_cast<std::size_t>(descriptor->PayloadSize()), vector.bytes.size()) << descriptor->name;
    }
}

TEST_F(FocMotorWireContractTest, the_server_accepts_every_command_golden_vector)
{
    EXPECT_CALL(serverObserver, OnStart(_)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnStop(_)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnClearFault(_)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnEmergencyStop(_)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnSelectControlMode(_, _)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnSetTorqueSetpoint(_, _)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnSetSpeedSetpoint(_, _)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnSetPositionSetpoint(_, _)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnSetPidCurrent(_, _)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnSetPidSpeed(_, _)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnSetPidPosition(_, _)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnAlign(_)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnIdentifyElectrical(_)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnIdentifyMechanical(_)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnRequestTelemetry(_)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnSetEncoderResolution(_, _)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnConfigureTelemetryRate(_, _)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnSelectAlgorithm(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnQueryAlgorithms(_)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnSetParameter(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(serverObserver, OnQueryParameter(_, _)).Times(AnyNumber());

    for (const auto& descriptor : can::focMotorMessages)
    {
        const auto vector = GoldenVectorFor(descriptor.id);
        ASSERT_TRUE(vector.has_value()) << descriptor.name;

        hal::Can::Message frame;
        for (const auto byte : vector->bytes)
            frame.push_back(byte);

        ackSpy.Reset();
        server.HandleMessage(descriptor.id, frame);

        if (ackSpy.last.has_value())
        {
            EXPECT_NE(services::CanAckStatus::invalidPayload, ackSpy.last->status) << descriptor.name;
        }
    }
}

TEST_F(FocMotorWireContractTest, an_overlong_category_error_frame_is_dropped_rather_than_reported)
{
    const auto valid = Bytes({ can::focStartId, static_cast<uint8_t>(can::FocMotorCategoryError::busy) });

    EXPECT_CALL(clientObserver, OnCategoryError(1, can::focStartId, can::FocMotorCategoryError::busy));
    client.HandleMessage(services::canCategoryErrorResponseMessageTypeId, valid);

    const auto overlong = Bytes({ can::focStartId, static_cast<uint8_t>(can::FocMotorCategoryError::busy), 0x00 });
    client.HandleMessage(services::canCategoryErrorResponseMessageTypeId, overlong);
}

TEST_F(FocMotorWireContractTest, every_response_golden_vector_reaches_the_client)
{
    EXPECT_CALL(clientObserver, OnAlgorithmsResponse(1, (can::FocAlgorithms{ can::FocMotorMode::speed, can::FocCurrentAlgorithm::pid, can::FocSpeedAlgorithm::adrc, can::FocPositionAlgorithm::pid })));
    client.HandleMessage(can::focAlgorithmsResponseId, Bytes({ 0x01, 0x00, 0x02, 0x00 }));

    EXPECT_CALL(clientObserver, OnParameterResponse(1, can::FocParameter::resistance, (can::FocParameterValue{ 0.36f, false })));
    client.HandleMessage(can::focParameterResponseId, Bytes({ 0x40, 0x00, 0x3E, 0xB8, 0x51, 0xEC }));
}

TEST_F(FocMotorWireContractTest, an_algorithms_response_naming_no_algorithm_is_dropped)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focAlgorithmsResponseId, Bytes({ 0x03, 0x00, 0x00, 0x00 })));
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focAlgorithmsResponseId, Bytes({ 0x00, 0x04, 0x00, 0x00 })));
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focAlgorithmsResponseId, Bytes({ 0x00, 0x00, 0x04, 0x00 })));
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focAlgorithmsResponseId, Bytes({ 0x00, 0x00, 0x00, 0x05 })));
}

TEST_F(FocMotorWireContractTest, a_parameter_response_naming_no_parameter_is_dropped)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focParameterResponseId, Bytes({ 0x05, 0x00, 0x3E, 0xB8, 0x51, 0xEC })));
}

TEST_F(FocMotorWireContractTest, parameter_numbers_are_unique_and_every_range_admits_its_bounds)
{
    std::set<uint8_t> numbers;
    for (const auto& parameter : can::focParameters)
    {
        EXPECT_TRUE(numbers.insert(static_cast<uint8_t>(parameter.id)).second) << parameter.name;
        EXPECT_LE(parameter.minimum, parameter.maximum) << parameter.name;
        EXPECT_TRUE(can::IsAcceptableParameterValue(parameter, parameter.maximum)) << parameter.name;
        EXPECT_TRUE(can::IsAcceptableParameterValue(parameter, parameter.minimum)) << parameter.name;
        EXPECT_EQ(parameter.automatic, can::IsAcceptableParameterValue(parameter, 0.0f) && parameter.minimum > 0.0f) << parameter.name;
    }
}

TEST_F(FocMotorWireContractTest, values_outside_a_parameter_s_domain_are_refused)
{
    const auto check = [](can::FocParameter id, float value)
    {
        const auto parameter = can::FindParameter(id);
        EXPECT_TRUE(parameter.has_value());
        return parameter.has_value() && can::IsAcceptableParameterValue(*parameter, value);
    };

    EXPECT_FALSE(check(can::FocParameter::speedObserverBandwidthRatio, 25.0f));
    EXPECT_FALSE(check(can::FocParameter::speedErrorWeight, 0.0f));
    EXPECT_FALSE(check(can::FocParameter::resistance, std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(check(can::FocParameter::inertia, std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(check(can::FocParameter::polePairs, 4.5f));
    EXPECT_FALSE(check(can::FocParameter::canBitrate, 300000.0f));
    EXPECT_FALSE(check(can::FocParameter::currentBandwidth, -1.0f));

    EXPECT_TRUE(check(can::FocParameter::currentBandwidth, 0.0f));
    EXPECT_TRUE(check(can::FocParameter::fluxLinkage, 0.0f));
    EXPECT_TRUE(check(can::FocParameter::canBitrate, 500000.0f));
    EXPECT_TRUE(check(can::FocParameter::polePairs, 7.0f));
    EXPECT_TRUE(check(can::FocParameter::viscousFriction, 0.0f));
}
