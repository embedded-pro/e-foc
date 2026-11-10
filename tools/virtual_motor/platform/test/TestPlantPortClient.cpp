#include "can-lite/drivers/implementation/LoopbackSocket.hpp"
#include "tools/virtual_motor/platform/PlantPortClient.hpp"
#include "tools/virtual_motor/port/PlantPortCodec.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <chrono>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
    using namespace virtual_motor::port;
    namespace loopback = services::loopback;

    class PlantPortTargetMock
        : public virtual_motor::PlantPortTarget
    {
    public:
        MOCK_METHOD(Hello, Identity, (), (const, override));
        MOCK_METHOD(Status, CurrentStatus, (), (const, override));
        MOCK_METHOD(Changes, ChangesInForce, (), (const, override));
        MOCK_METHOD(void, Pause, (), (override));
        MOCK_METHOD(void, Resume, (), (override));
        MOCK_METHOD(std::optional<RefusalReason>, Step, (StepUnit unit, uint16_t count), (override));
        MOCK_METHOD(std::optional<RefusalReason>, SetLoadTorque, (float newtonMeter), (override));
        MOCK_METHOD(std::optional<RefusalReason>, SetExternalTorque, (const ExternalTorque& torque), (override));
        MOCK_METHOD(std::optional<RefusalReason>, SetCurrentNoise, (float ampere), (override));
        MOCK_METHOD(std::optional<RefusalReason>, SetEncoderNoise, (float radians), (override));
        MOCK_METHOD(void, ClearAll, (), (override));
    };

    class TestPlantPortClient
        : public testing::Test
    {
    public:
        TestPlantPortClient()
        {
            EXPECT_CALL(target, Identity()).WillRepeatedly(testing::Return(Hello{ protocolVersion, 3, "bench", "teknic", 20000 }));
            EXPECT_CALL(target, CurrentStatus()).WillRepeatedly(testing::Return(Status{ 40, 1.0f, 0, RunState::running, led::operational, 0 }));
            EXPECT_CALL(target, ChangesInForce()).WillRepeatedly(testing::Return(Changes{}));
        }

        ~TestPlantPortClient() override
        {
            loopback::Close(app);
            loopback::Close(listener);
        }

        void Attach()
        {
            ASSERT_TRUE(client.Connect(loopback::LocalPort(listener).value_or(0)));
            app = loopback::Accept(listener);
            ASSERT_NE(app, loopback::invalidSocket);
            ASSERT_EQ(Received(3).size(), 3u);
        }

        void SendToMotor(const std::vector<uint8_t>& bytes)
        {
            ASSERT_TRUE(loopback::Send(app, infra::ConstByteRange{ bytes.data(), bytes.data() + bytes.size() }));
            for (int attempt = 0; attempt != 100; ++attempt)
            {
                client.Poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        std::vector<Frame> Received(std::size_t expected)
        {
            std::vector<Frame> frames;
            std::array<uint8_t, 4096> buffer{};
            for (int attempt = 0; attempt != 500 && frames.size() < expected; ++attempt)
            {
                const auto size = loopback::Receive(app, infra::ByteRange{ buffer.data(), buffer.data() + buffer.size() });
                if (size)
                    assembler.Append(buffer.data(), *size);
                while (auto frame = assembler.Next())
                    frames.push_back(*frame);
                if (frames.size() < expected)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return frames;
        }

        testing::StrictMock<PlantPortTargetMock> target;
        intptr_t listener{ loopback::Listen(0) };
        intptr_t app{ loopback::invalidSocket };
        FrameAssembler assembler;
        virtual_motor::PlantPortClient client{ target };
    };
}

TEST_F(TestPlantPortClient, the_motor_introduces_itself_then_its_changes_and_status)
{
    ASSERT_TRUE(client.Connect(loopback::LocalPort(listener).value_or(0)));
    app = loopback::Accept(listener);

    const auto frames = Received(3);
    ASSERT_EQ(frames.size(), 3u);
    EXPECT_EQ(Decode<Hello>(frames[0])->bus, "bench");
    EXPECT_TRUE(Decode<Changes>(frames[1]).has_value());
    EXPECT_EQ(Decode<Status>(frames[2])->controlPeriods, 40u);
    EXPECT_TRUE(client.IsConnected());
}

TEST_F(TestPlantPortClient, without_an_app_listening_the_port_does_not_connect)
{
    const auto port = loopback::LocalPort(listener).value_or(0);
    loopback::Close(listener);
    listener = loopback::invalidSocket;

    EXPECT_FALSE(client.Connect(port));
    EXPECT_FALSE(client.IsConnected());
}

TEST_F(TestPlantPortClient, pause_and_resume_answer_with_the_status)
{
    Attach();

    EXPECT_CALL(target, Pause());
    SendToMotor(Encode(Pause{}));
    EXPECT_TRUE(Decode<Status>(Received(1).at(0)).has_value());

    EXPECT_CALL(target, Resume());
    SendToMotor(Encode(Resume{}));
    EXPECT_TRUE(Decode<Status>(Received(1).at(0)).has_value());
}

TEST_F(TestPlantPortClient, a_finished_step_answers_with_the_status_and_a_refused_one_with_the_reason)
{
    Attach();

    EXPECT_CALL(target, Step(StepUnit::millisecond, 5)).WillOnce(testing::Return(std::nullopt));
    SendToMotor(Encode(Step{ StepUnit::millisecond, 5 }));
    EXPECT_TRUE(Decode<Status>(Received(1).at(0)).has_value());

    EXPECT_CALL(target, Step(StepUnit::controlPeriod, 1)).WillOnce(testing::Return(RefusalReason::stepWhileRunning));
    SendToMotor(Encode(Step{ StepUnit::controlPeriod, 1 }));
    const auto refused = Decode<Refused>(Received(1).at(0));
    ASSERT_TRUE(refused.has_value());
    EXPECT_EQ(refused->request, static_cast<uint8_t>(MessageType::step));
    EXPECT_EQ(refused->reason, RefusalReason::stepWhileRunning);
}

TEST_F(TestPlantPortClient, an_accepted_change_answers_with_the_changes_in_force)
{
    Attach();

    EXPECT_CALL(target, SetLoadTorque(0.25f)).WillOnce(testing::Return(std::nullopt));
    SendToMotor(Encode(LoadTorque{ 0.25f }));
    EXPECT_TRUE(Decode<Changes>(Received(1).at(0)).has_value());

    EXPECT_CALL(target, SetExternalTorque(testing::Field(&ExternalTorque::form, TorqueForm::sine))).WillOnce(testing::Return(std::nullopt));
    SendToMotor(Encode(ExternalTorqueRequest{ ExternalTorque{ TorqueForm::sine, { 0.0f, 0.1f, 5.0f } } }));
    EXPECT_TRUE(Decode<Changes>(Received(1).at(0)).has_value());

    EXPECT_CALL(target, SetCurrentNoise(0.05f)).WillOnce(testing::Return(std::nullopt));
    SendToMotor(Encode(CurrentNoise{ 0.05f }));
    EXPECT_TRUE(Decode<Changes>(Received(1).at(0)).has_value());

    EXPECT_CALL(target, ClearAll());
    SendToMotor(Encode(ClearAll{}));
    EXPECT_TRUE(Decode<Changes>(Received(1).at(0)).has_value());
}

TEST_F(TestPlantPortClient, a_refused_change_answers_with_the_reason)
{
    Attach();

    EXPECT_CALL(target, SetEncoderNoise(-1.0f)).WillOnce(testing::Return(RefusalReason::outOfRange));
    SendToMotor(Encode(EncoderNoise{ -1.0f }));

    const auto refused = Decode<Refused>(Received(1).at(0));
    ASSERT_TRUE(refused.has_value());
    EXPECT_EQ(refused->request, static_cast<uint8_t>(MessageType::encoderNoise));
    EXPECT_EQ(refused->reason, RefusalReason::outOfRange);
}

TEST_F(TestPlantPortClient, every_debug_log_line_becomes_one_frame)
{
    Attach();

    constexpr std::string_view text{ "first line\r\nsecond line\nthird" };
    client.Log(infra::ConstByteRange{ reinterpret_cast<const uint8_t*>(text.data()), reinterpret_cast<const uint8_t*>(text.data() + text.size()) });

    const auto frames = Received(2);
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_EQ(Decode<LogLine>(frames[0])->text, "first line");
    EXPECT_EQ(Decode<LogLine>(frames[1])->text, "second line");
}

TEST_F(TestPlantPortClient, the_status_is_published_every_100_milliseconds)
{
    Attach();

    for (int millisecond = 0; millisecond != 199; ++millisecond)
        client.Millisecond();
    EXPECT_EQ(Received(2).size(), 1u);

    client.Millisecond();
    EXPECT_EQ(Received(1).size(), 1u);
}

TEST_F(TestPlantPortClient, an_unknown_request_is_skipped)
{
    Attach();

    SendToMotor(EncodeFrame(0xfe, { 1, 2, 3 }));

    EXPECT_TRUE(client.IsConnected());
}

TEST_F(TestPlantPortClient, losing_the_app_resumes_the_motor_and_ends_the_port)
{
    Attach();

    EXPECT_CALL(target, Resume());
    loopback::Close(app);
    app = loopback::invalidSocket;
    for (int attempt = 0; attempt != 100 && client.IsConnected(); ++attempt)
    {
        client.Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_FALSE(client.IsConnected());
}

TEST_F(TestPlantPortClient, a_request_shorter_than_its_fields_ends_the_port)
{
    Attach();

    EXPECT_CALL(target, Resume());
    SendToMotor(EncodeFrame(static_cast<uint8_t>(MessageType::loadTorque), { 0 }));

    EXPECT_FALSE(client.IsConnected());
}
