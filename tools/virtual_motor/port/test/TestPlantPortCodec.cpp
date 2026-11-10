#include "tools/virtual_motor/port/PlantPortCodec.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <string>
#include <vector>

namespace
{
    using namespace virtual_motor::port;

    template<class Message>
    Message RoundTrip(const Message& message)
    {
        FrameAssembler assembler;
        assembler.Append(Encode(message));
        const auto frame = assembler.Next();
        EXPECT_TRUE(frame.has_value());
        EXPECT_EQ(frame->type, static_cast<uint8_t>(Message::type));
        const auto decoded = Decode<Message>(*frame);
        EXPECT_TRUE(decoded.has_value());
        return decoded.value_or(Message{});
    }
}

TEST(TestPlantPortCodec, a_frame_is_its_type_its_little_endian_length_and_its_payload)
{
    EXPECT_THAT(Encode(LoadTorque{ 1.5f }), testing::ElementsAre(0x84, 4, 0, 0x00, 0x00, 0xc0, 0x3f));
    EXPECT_THAT(Encode(Pause{}), testing::ElementsAre(0x81, 0, 0));
}

TEST(TestPlantPortCodec, hello_carries_version_node_bus_plant_and_control_rate)
{
    const auto hello = RoundTrip(Hello{ protocolVersion, 7, "bench", "teknic", 20000 });

    EXPECT_EQ(hello.version, protocolVersion);
    EXPECT_EQ(hello.node, 7);
    EXPECT_EQ(hello.bus, "bench");
    EXPECT_EQ(hello.plant, "teknic");
    EXPECT_EQ(hello.controlRateHz, 20000u);
}

TEST(TestPlantPortCodec, status_carries_time_pacing_leds_and_protection)
{
    const auto status = RoundTrip(Status{ 0x1'0000'0001ull, 0.99f, 3, RunState::paused, 0b101, 1 });

    EXPECT_EQ(status.controlPeriods, 0x1'0000'0001ull);
    EXPECT_FLOAT_EQ(status.realTimeFactor, 0.99f);
    EXPECT_EQ(status.overruns, 3u);
    EXPECT_EQ(status.runState, RunState::paused);
    EXPECT_EQ(status.leds, 0b101);
    EXPECT_EQ(status.protection, 1);
}

TEST(TestPlantPortCodec, changes_carry_every_value_in_force_and_which_differ)
{
    const auto changes = RoundTrip(Changes{ 0.1f, ExternalTorque{ TorqueForm::sine, { 0.01f, 0.05f, 2.0f, 0.0f } }, 0.02f, 0.001f, 0b0110 });

    EXPECT_FLOAT_EQ(changes.loadTorque, 0.1f);
    EXPECT_EQ(changes.externalTorque.form, TorqueForm::sine);
    EXPECT_THAT(changes.externalTorque.parameters, testing::ElementsAre(0.01f, 0.05f, 2.0f, 0.0f));
    EXPECT_FLOAT_EQ(changes.currentNoise, 0.02f);
    EXPECT_FLOAT_EQ(changes.encoderNoise, 0.001f);
    EXPECT_EQ(changes.differs, 0b0110);
}

TEST(TestPlantPortCodec, requests_and_refusals_round_trip)
{
    EXPECT_EQ(RoundTrip(Step{ StepUnit::millisecond, 300 }).count, 300);
    EXPECT_EQ(RoundTrip(Step{ StepUnit::millisecond, 300 }).unit, StepUnit::millisecond);
    EXPECT_FLOAT_EQ(RoundTrip(CurrentNoise{ 0.03f }).ampere, 0.03f);
    EXPECT_FLOAT_EQ(RoundTrip(EncoderNoise{ 0.002f }).radians, 0.002f);
    EXPECT_EQ(RoundTrip(ExternalTorqueRequest{ ExternalTorque{ TorqueForm::ramp, { 0.0f, 0.1f, 0.5f, 0.0f } } }).torque.form, TorqueForm::ramp);
    EXPECT_EQ(RoundTrip(Refused{ static_cast<uint8_t>(MessageType::step), RefusalReason::stepWhileRunning }).reason, RefusalReason::stepWhileRunning);
    EXPECT_EQ(RoundTrip(LogLine{ "fsm: handled Start in Ready" }).text, "fsm: handled Start in Ready");
}

TEST(TestPlantPortCodec, a_text_longer_than_its_length_byte_is_cut_at_255_characters)
{
    EXPECT_EQ(RoundTrip(LogLine{ std::string(300, 'x') }).text, std::string(255, 'x'));
}

TEST(TestPlantPortCodec, the_assembler_waits_for_a_whole_frame)
{
    const auto bytes = Encode(LoadTorque{ 2.0f });
    FrameAssembler assembler;

    for (std::size_t index = 0; index + 1 < bytes.size(); ++index)
    {
        assembler.Append({ bytes[index] });
        EXPECT_FALSE(assembler.Next().has_value());
    }

    assembler.Append({ bytes.back() });
    EXPECT_TRUE(assembler.Next().has_value());
    EXPECT_FALSE(assembler.Next().has_value());
}

TEST(TestPlantPortCodec, the_assembler_splits_frames_that_arrive_together)
{
    auto bytes = Encode(Pause{});
    const auto second = Encode(EncoderNoise{ 0.5f });
    bytes.insert(bytes.end(), second.begin(), second.end());
    FrameAssembler assembler;
    assembler.Append(bytes);

    EXPECT_EQ(assembler.Next()->type, static_cast<uint8_t>(MessageType::pause));
    EXPECT_EQ(assembler.Next()->type, static_cast<uint8_t>(MessageType::encoderNoise));
    EXPECT_FALSE(assembler.Next().has_value());
}

TEST(TestPlantPortCodec, a_frame_of_an_unknown_type_is_delivered_for_the_reader_to_skip)
{
    FrameAssembler assembler;
    assembler.Append({ 0x7e, 2, 0, 0xaa, 0xbb });

    const auto frame = assembler.Next();
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->type, 0x7e);
    EXPECT_THAT(frame->payload, testing::ElementsAre(0xaa, 0xbb));
}

TEST(TestPlantPortCodec, a_payload_shorter_than_its_fields_does_not_decode)
{
    EXPECT_FALSE(Decode<LoadTorque>(Frame{ static_cast<uint8_t>(MessageType::loadTorque), { 0x00, 0x00 } }).has_value());
    EXPECT_FALSE(Decode<Hello>(Frame{ static_cast<uint8_t>(MessageType::hello), { 1, 0, 3, 5, 'a' } }).has_value());
}

TEST(TestPlantPortCodec, a_frame_of_another_type_does_not_decode)
{
    EXPECT_FALSE(Decode<CurrentNoise>(Frame{ static_cast<uint8_t>(MessageType::encoderNoise), { 0, 0, 0, 0 } }).has_value());
}
