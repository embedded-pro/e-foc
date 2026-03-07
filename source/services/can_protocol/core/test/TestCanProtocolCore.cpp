#include "source/services/can_protocol/core/CanCategoryHandler.hpp"
#include "source/services/can_protocol/core/CanFrameCodec.hpp"
#include "source/services/can_protocol/core/CanProtocolDefinitions.hpp"
#include "gtest/gtest.h"
#include <limits>

namespace
{
    using namespace services;

    // --- CanCategoryHandler base class ---

    class StubCategoryHandler : public CanCategoryHandler
    {
    public:
        explicit StubCategoryHandler(CanCategory cat)
            : cat(cat)
        {}

        CanCategory Category() const override
        {
            return cat;
        }

        void Handle(CanMessageType messageType, const hal::Can::Message& data) override
        {
            lastMessageType = messageType;
            lastDataSize = data.size();
            handleCallCount++;
        }

        CanMessageType lastMessageType{};
        std::size_t lastDataSize = 0;
        int handleCallCount = 0;

    private:
        CanCategory cat;
    };

    class NoSequenceCategoryHandler : public CanCategoryHandler
    {
    public:
        CanCategory Category() const override
        {
            return CanCategory::system;
        }

        bool RequiresSequenceValidation() const override
        {
            return false;
        }

        void Handle(CanMessageType, const hal::Can::Message&) override
        {}
    };

    TEST(CanCategoryHandlerTest, DefaultRequiresSequenceValidation)
    {
        StubCategoryHandler handler(CanCategory::motorControl);
        EXPECT_TRUE(handler.RequiresSequenceValidation());
    }

    TEST(CanCategoryHandlerTest, OverriddenSequenceValidation)
    {
        NoSequenceCategoryHandler handler;
        EXPECT_FALSE(handler.RequiresSequenceValidation());
    }

    TEST(CanCategoryHandlerTest, CategoryReturnsConfiguredValue)
    {
        StubCategoryHandler motorHandler(CanCategory::motorControl);
        StubCategoryHandler pidHandler(CanCategory::pidTuning);
        StubCategoryHandler sysParamHandler(CanCategory::systemParameters);
        StubCategoryHandler sysHandler(CanCategory::system);

        EXPECT_EQ(motorHandler.Category(), CanCategory::motorControl);
        EXPECT_EQ(pidHandler.Category(), CanCategory::pidTuning);
        EXPECT_EQ(sysParamHandler.Category(), CanCategory::systemParameters);
        EXPECT_EQ(sysHandler.Category(), CanCategory::system);
    }

    TEST(CanCategoryHandlerTest, HandleDeliversMessageTypeAndData)
    {
        StubCategoryHandler handler(CanCategory::motorControl);
        hal::Can::Message msg;
        msg.push_back(0x01);
        msg.push_back(0x02);
        msg.push_back(0x03);

        handler.Handle(CanMessageType::startMotor, msg);

        EXPECT_EQ(handler.lastMessageType, CanMessageType::startMotor);
        EXPECT_EQ(handler.lastDataSize, 3u);
        EXPECT_EQ(handler.handleCallCount, 1);
    }

    TEST(CanCategoryHandlerTest, HandleCanBeCalledMultipleTimes)
    {
        StubCategoryHandler handler(CanCategory::pidTuning);
        hal::Can::Message msg;

        handler.Handle(CanMessageType::setCurrentIdPid, msg);
        handler.Handle(CanMessageType::setSpeedPid, msg);

        EXPECT_EQ(handler.handleCallCount, 2);
        EXPECT_EQ(handler.lastMessageType, CanMessageType::setSpeedPid);
    }

    // --- CanId construction and extraction ---

    TEST(CanIdTest, RoundTrip)
    {
        uint32_t id = MakeCanId(CanPriority::command, CanCategory::motorControl, CanMessageType::startMotor, 42);

        EXPECT_EQ(ExtractCanPriority(id), CanPriority::command);
        EXPECT_EQ(ExtractCanCategory(id), CanCategory::motorControl);
        EXPECT_EQ(ExtractCanMessageType(id), CanMessageType::startMotor);
        EXPECT_EQ(ExtractCanNodeId(id), 42);
    }

    TEST(CanIdTest, AllPriorities)
    {
        for (auto p : { CanPriority::emergency, CanPriority::command, CanPriority::telemetry, CanPriority::heartbeat })
        {
            uint32_t id = MakeCanId(p, CanCategory::system, CanMessageType::heartbeat, 1);
            EXPECT_EQ(ExtractCanPriority(id), p);
        }
    }

    TEST(CanIdTest, AllCategories)
    {
        for (auto c : { CanCategory::motorControl, CanCategory::pidTuning, CanCategory::systemParameters, CanCategory::telemetry, CanCategory::system })
        {
            uint32_t id = MakeCanId(CanPriority::command, c, CanMessageType::startMotor, 1);
            EXPECT_EQ(ExtractCanCategory(id), c);
        }
    }

    TEST(CanIdTest, BroadcastNodeId)
    {
        uint32_t id = MakeCanId(CanPriority::emergency, CanCategory::motorControl, CanMessageType::emergencyStop, canBroadcastNodeId);
        EXPECT_EQ(ExtractCanNodeId(id), canBroadcastNodeId);
    }

    TEST(CanIdTest, MaxNodeId)
    {
        uint32_t id = MakeCanId(CanPriority::command, CanCategory::motorControl, CanMessageType::startMotor, 0xFFF);
        EXPECT_EQ(ExtractCanNodeId(id), 0xFFF);
    }

    // --- CanFrameCodec ---

    TEST(CanFrameCodecTest, FloatToFixed16_NormalValue)
    {
        auto result = CanFrameCodec::FloatToFixed16(1.5f, 1000);
        EXPECT_EQ(result, 1500);
    }

    TEST(CanFrameCodecTest, FloatToFixed16_NegativeValue)
    {
        auto result = CanFrameCodec::FloatToFixed16(-2.5f, 1000);
        EXPECT_EQ(result, -2500);
    }

    TEST(CanFrameCodecTest, FloatToFixed16_SaturatesAtMax)
    {
        auto result = CanFrameCodec::FloatToFixed16(100.0f, 1000);
        EXPECT_EQ(result, std::numeric_limits<int16_t>::max());
    }

    TEST(CanFrameCodecTest, FloatToFixed16_SaturatesAtMin)
    {
        auto result = CanFrameCodec::FloatToFixed16(-100.0f, 1000);
        EXPECT_EQ(result, std::numeric_limits<int16_t>::min());
    }

    TEST(CanFrameCodecTest, FloatToFixed16_RoundsToNearest)
    {
        auto result = CanFrameCodec::FloatToFixed16(1.2345f, 1000);
        EXPECT_EQ(result, 1235);
    }

    TEST(CanFrameCodecTest, Fixed16ToFloat_RoundTrip)
    {
        int16_t fixed = CanFrameCodec::FloatToFixed16(3.14f, 1000);
        float result = CanFrameCodec::Fixed16ToFloat(fixed, 1000);
        EXPECT_NEAR(result, 3.14f, 0.01f);
    }

    TEST(CanFrameCodecTest, FloatToFixed32_NormalValue)
    {
        auto result = CanFrameCodec::FloatToFixed32(1.5f, 1000);
        EXPECT_EQ(result, 1500);
    }

    TEST(CanFrameCodecTest, FloatToFixed32_SaturatesAtMax)
    {
        auto result = CanFrameCodec::FloatToFixed32(1e15f, 1000);
        EXPECT_EQ(result, std::numeric_limits<int32_t>::max());
    }

    TEST(CanFrameCodecTest, FloatToFixed32_SaturatesAtMin)
    {
        auto result = CanFrameCodec::FloatToFixed32(-1e15f, 1000);
        EXPECT_EQ(result, std::numeric_limits<int32_t>::min());
    }

    TEST(CanFrameCodecTest, FloatToFixed32_RoundsToNearest)
    {
        auto result = CanFrameCodec::FloatToFixed32(1.2345f, 1000);
        EXPECT_EQ(result, 1235);
    }

    TEST(CanFrameCodecTest, Fixed32ToFloat_RoundTrip)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(3.14f, 10000);
        float result = CanFrameCodec::Fixed32ToFloat(fixed, 10000);
        EXPECT_NEAR(result, 3.14f, 0.001f);
    }

    TEST(CanFrameCodecTest, WriteAndReadInt16_RoundTrip)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt16(msg, 0, 12345);
        EXPECT_EQ(CanFrameCodec::ReadInt16(msg, 0), 12345);
    }

    TEST(CanFrameCodecTest, WriteAndReadInt16_NegativeValue)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt16(msg, 0, -9876);
        EXPECT_EQ(CanFrameCodec::ReadInt16(msg, 0), -9876);
    }

    TEST(CanFrameCodecTest, WriteAndReadInt32_RoundTrip)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt32(msg, 0, 123456);
        EXPECT_EQ(CanFrameCodec::ReadInt32(msg, 0), 123456);
    }

    TEST(CanFrameCodecTest, WriteAndReadInt32_NegativeValue)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt32(msg, 0, -987654);
        EXPECT_EQ(CanFrameCodec::ReadInt32(msg, 0), -987654);
    }

    TEST(CanFrameCodecTest, WriteInt16_AtOffset)
    {
        hal::Can::Message msg;
        msg.push_back(0xAA);
        CanFrameCodec::WriteInt16(msg, 1, 256);
        EXPECT_EQ(msg[0], 0xAA);
        EXPECT_EQ(CanFrameCodec::ReadInt16(msg, 1), 256);
    }

    TEST(CanFrameCodecTest, WriteInt32_AtOffset)
    {
        hal::Can::Message msg;
        msg.push_back(0xBB);
        CanFrameCodec::WriteInt32(msg, 1, 65536);
        EXPECT_EQ(msg[0], 0xBB);
        EXPECT_EQ(CanFrameCodec::ReadInt32(msg, 1), 65536);
    }

    // --- DataRequestFlags ---

    TEST(DataRequestFlagsTest, HasFlag)
    {
        EXPECT_TRUE(HasFlag(DataRequestFlags::all, DataRequestFlags::motorStatus));
        EXPECT_TRUE(HasFlag(DataRequestFlags::all, DataRequestFlags::currentMeasurement));
        EXPECT_TRUE(HasFlag(DataRequestFlags::all, DataRequestFlags::speedPosition));
        EXPECT_TRUE(HasFlag(DataRequestFlags::all, DataRequestFlags::busVoltage));
        EXPECT_TRUE(HasFlag(DataRequestFlags::all, DataRequestFlags::faultEvent));
    }

    TEST(DataRequestFlagsTest, SingleFlags)
    {
        EXPECT_TRUE(HasFlag(DataRequestFlags::motorStatus, DataRequestFlags::motorStatus));
        EXPECT_FALSE(HasFlag(DataRequestFlags::motorStatus, DataRequestFlags::currentMeasurement));
    }

    TEST(DataRequestFlagsTest, BitwiseOr)
    {
        auto combined = DataRequestFlags::motorStatus | DataRequestFlags::busVoltage;
        EXPECT_TRUE(HasFlag(combined, DataRequestFlags::motorStatus));
        EXPECT_TRUE(HasFlag(combined, DataRequestFlags::busVoltage));
        EXPECT_FALSE(HasFlag(combined, DataRequestFlags::speedPosition));
    }

    TEST(DataRequestFlagsTest, BitwiseAnd)
    {
        auto result = DataRequestFlags::all & DataRequestFlags::motorStatus;
        EXPECT_EQ(result, DataRequestFlags::motorStatus);
    }
}
