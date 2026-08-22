#pragma GCC optimize("O3", "fast-math")

#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "infra/util/Function.hpp"
#include <gtest/gtest.h>
#include <optional>

namespace
{
    using namespace testing;

    struct AcknowledgerSpy : services::CanCommandAcknowledger
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

        void Reset() { last.reset(); }
    };

    class MockServerObserver : public can::FocMotorCategoryServerObserver
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
        MOCK_METHOD(void, OnIdentifyElectrical, (const infra::Function<void()>&), (override));
        MOCK_METHOD(void, OnIdentifyMechanical, (const infra::Function<void()>&), (override));
        MOCK_METHOD(void, OnRequestTelemetry, (const infra::Function<void()>&), (override));
        MOCK_METHOD(void, OnSetEncoderResolution, (uint32_t, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnConfigureTelemetryRate, (uint32_t, (const infra::Function<void()>&)), (override));
    };

    class FocMotorCategoryServerTest : public Test
    {
    public:
        FocMotorCategoryServerTest()
        {
            EXPECT_CALL(canMock, SendData(_, _, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([this](hal::Can::Id id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                    {
                        lastSentMsgType = services::ExtractCanMessageType(id.Get29BitId());
                        lastSentMsg = msg;
                        if (lastSentMsgType == services::canCategoryErrorResponseMessageTypeId && msg.size() >= 2)
                        {
                            categoryErrorSent = true;
                            lastCategoryErrorOriginCmd = msg[0];
                            lastCategoryError = static_cast<can::FocMotorCategoryError>(msg[1]);
                        }
                        cb(true);
                    }));
            server.SetAcknowledger(ackSpy);
        }

        hal::Can::Message MakePayload(uint8_t seqByte)
        {
            hal::Can::Message msg;
            msg.push_back(seqByte);
            return msg;
        }

        hal::Can::Message MakeSetpointPayload(int16_t wireValue)
        {
            hal::Can::Message msg;
            msg.resize(3, 0);
            services::CanFrameCodec::WriteInt16(msg, 1, wireValue);
            return msg;
        }

        hal::Can::Message MakeModePayload(can::FocMotorMode mode)
        {
            hal::Can::Message msg;
            msg.resize(2, 0);
            msg[1] = static_cast<uint8_t>(mode);
            return msg;
        }

        hal::Can::Message MakeUInt32Payload(uint32_t value)
        {
            hal::Can::Message msg;
            msg.resize(5, 0);
            services::CanFrameCodec::WriteUInt32(msg, 1, value);
            return msg;
        }

        StrictMock<hal::CanMock> canMock;
        services::CanFrameTransport transport{ canMock, 1 };
        can::FocMotorCategoryServer server{ transport };
        AcknowledgerSpy ackSpy;
        StrictMock<MockServerObserver> observer{ server };

        uint8_t lastSentMsgType{};
        hal::Can::Message lastSentMsg;
        bool categoryErrorSent{ false };
        uint8_t lastCategoryErrorOriginCmd{};
        can::FocMotorCategoryError lastCategoryError{ can::FocMotorCategoryError::busy };
    };

    TEST_F(FocMotorCategoryServerTest, CategoryId_IsFocMotorCategoryId)
    {
        EXPECT_EQ(server.Id(), can::focMotorCategoryId);
    }

    TEST_F(FocMotorCategoryServerTest, HandleStart_InvokesOnStartObserver)
    {
        EXPECT_CALL(observer, OnStart(_))
            .WillOnce(Invoke([](const infra::Function<void(services::CanAckStatus)>& ack)
                {
                    ack(services::CanAckStatus::success);
                }));

        server.HandleMessage(can::focStartId, MakePayload(1));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->commandType, can::focStartId);
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCategoryServerTest, HandleStop_InvokesOnStopObserver)
    {
        EXPECT_CALL(observer, OnStop(_))
            .WillOnce(Invoke([](const infra::Function<void(services::CanAckStatus)>& ack)
                {
                    ack(services::CanAckStatus::success);
                }));

        server.HandleMessage(can::focStopId, MakePayload(1));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCategoryServerTest, HandleSetTorqueSetpoint_DecodesCurrentCorrectly)
    {
        foc::Ampere received{ 0.0f };
        EXPECT_CALL(observer, OnSetTorqueSetpoint(_, _))
            .WillOnce(Invoke([&received](foc::Ampere val, const infra::Function<void()>& done)
                {
                    received = val;
                    done();
                }));

        server.HandleMessage(can::focSetTorqueSetpointId, MakeSetpointPayload(15));

        EXPECT_NEAR(received.Value(), 15.0f / can::focCurrentScale, 0.001f);
    }

    TEST_F(FocMotorCategoryServerTest, HandleSetSpeedSetpoint_DecodesSpeedCorrectly)
    {
        foc::RadiansPerSecond received{ 0.0f };
        EXPECT_CALL(observer, OnSetSpeedSetpoint(_, _))
            .WillOnce(Invoke([&received](foc::RadiansPerSecond val, const infra::Function<void()>& done)
                {
                    received = val;
                    done();
                }));

        server.HandleMessage(can::focSetSpeedSetpointId, MakeSetpointPayload(300));

        EXPECT_NEAR(received.Value(), 300.0f / can::focSpeedScale, 0.01f);
    }

    TEST_F(FocMotorCategoryServerTest, HandleSetPositionSetpoint_DecodesPositionCorrectly)
    {
        foc::Radians received{ 0.0f };
        EXPECT_CALL(observer, OnSetPositionSetpoint(_, _))
            .WillOnce(Invoke([&received](foc::Radians val, const infra::Function<void()>& done)
                {
                    received = val;
                    done();
                }));

        server.HandleMessage(can::focSetPositionSetpointId, MakeSetpointPayload(314));

        EXPECT_NEAR(received.Value(), 314.0f / can::focPositionScale, 0.001f);
    }

    TEST_F(FocMotorCategoryServerTest, HandleSelectControlMode_ValidMode_InvokesObserver)
    {
        can::FocMotorMode received{ can::FocMotorMode::torque };
        EXPECT_CALL(observer, OnSelectControlMode(_, _))
            .WillOnce(Invoke([&received](can::FocMotorMode mode, const infra::Function<void(can::FocMotorMode)>& cb)
                {
                    received = mode;
                    cb(mode);
                }));

        server.HandleMessage(can::focSelectControlModeId, MakeModePayload(can::FocMotorMode::speed));

        EXPECT_EQ(received, can::FocMotorMode::speed);
        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
        EXPECT_EQ(lastSentMsgType, can::focSelectControlModeResponseId);
    }

    TEST_F(FocMotorCategoryServerTest, HandleSelectControlMode_InvalidMode_RejectsWithInvalidPayload)
    {
        hal::Can::Message msg;
        msg.resize(2, 0);
        msg[1] = 0xFF;
        server.HandleMessage(can::focSelectControlModeId, msg);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
    }

    TEST_F(FocMotorCategoryServerTest, SendCategoryError_EmitsCategoryErrorFrame)
    {
        server.SendCategoryError(can::focSetPidCurrentId, can::FocMotorCategoryError::applicationError);

        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryErrorOriginCmd, can::focSetPidCurrentId);
        EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::applicationError);
        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::categoryError);
    }

    TEST_F(FocMotorCategoryServerTest, HandleQueryMotorType_AcksNotImplemented)
    {
        server.HandleMessage(can::focQueryMotorTypeId, MakePayload(1));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::notImplemented);
    }

    TEST_F(FocMotorCategoryServerTest, HandleSetPidCurrent_ParsesBandwidthAndInvokesObserver)
    {
        EXPECT_CALL(observer, OnSetPidCurrent(_, _))
            .WillOnce(Invoke([](float, const infra::Function<void()>& done) { done(); }));

        server.HandleMessage(can::focSetPidCurrentId, MakeSetpointPayload(1000));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCategoryServerTest, HandleClearFault_InvokesOnClearFaultObserver)
    {
        EXPECT_CALL(observer, OnClearFault(_))
            .WillOnce(Invoke([](const infra::Function<void(services::CanAckStatus)>& ack)
                {
                    ack(services::CanAckStatus::success);
                }));

        server.HandleMessage(can::focClearFaultId, MakePayload(1));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCategoryServerTest, HandleEmergencyStop_InvokesOnEmergencyStopObserver)
    {
        EXPECT_CALL(observer, OnEmergencyStop(_))
            .WillOnce(Invoke([](const infra::Function<void(services::CanAckStatus)>& ack)
                {
                    ack(services::CanAckStatus::invalidState);
                }));

        server.HandleMessage(can::focEmergencyStopId, MakePayload(1));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
    }

    TEST_F(FocMotorCategoryServerTest, HandleSelectControlMode_InvalidMode_AcksInvalidPayload)
    {
        hal::Can::Message data;
        data.push_back(0);
        data.push_back(0xFF);
        server.HandleMessage(can::focSelectControlModeId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
    }

    TEST_F(FocMotorCategoryServerTest, HandleSetTorqueSetpoint_ShortPayload_AcksInvalidPayload)
    {
        server.HandleMessage(can::focSetTorqueSetpointId, MakePayload(1));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
    }

    TEST_F(FocMotorCategoryServerTest, HandleSetSpeedSetpoint_ShortPayload_AcksInvalidPayload)
    {
        server.HandleMessage(can::focSetSpeedSetpointId, MakePayload(1));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
    }

    TEST_F(FocMotorCategoryServerTest, HandleSetPositionSetpoint_ShortPayload_AcksInvalidPayload)
    {
        server.HandleMessage(can::focSetPositionSetpointId, MakePayload(1));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
    }

    TEST_F(FocMotorCategoryServerTest, HandleIdentifyElectrical_InvokesObserver)
    {
        EXPECT_CALL(observer, OnIdentifyElectrical(_))
            .WillOnce(Invoke([](const infra::Function<void()>& done) { done(); }));

        server.HandleMessage(can::focIdentifyElectricalId, MakePayload(1));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCategoryServerTest, HandleIdentifyMechanical_InvokesObserver)
    {
        EXPECT_CALL(observer, OnIdentifyMechanical(_))
            .WillOnce(Invoke([](const infra::Function<void()>& done) { done(); }));

        server.HandleMessage(can::focIdentifyMechanicalId, MakePayload(1));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCategoryServerTest, HandleRequestTelemetry_InvokesObserver)
    {
        EXPECT_CALL(observer, OnRequestTelemetry(_))
            .WillOnce(Invoke([](const infra::Function<void()>& done) { done(); }));

        server.HandleMessage(can::focRequestTelemetryId, MakePayload(1));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCategoryServerTest, HandleSetEncoderResolution_ParsesResolutionAndInvokesObserver)
    {
        EXPECT_CALL(observer, OnSetEncoderResolution(4000u, _))
            .WillOnce(Invoke([](uint32_t, const infra::Function<void()>& done) { done(); }));

        server.HandleMessage(can::focSetEncoderResolutionId, MakeUInt32Payload(4000));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCategoryServerTest, HandleConfigureTelemetryRate_ParsesRateAndInvokesObserver)
    {
        EXPECT_CALL(observer, OnConfigureTelemetryRate(100u, _))
            .WillOnce(Invoke([](uint32_t, const infra::Function<void()>& done) { done(); }));

        server.HandleMessage(can::focConfigureTelemetryRateId, MakeUInt32Payload(100));

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }
}
