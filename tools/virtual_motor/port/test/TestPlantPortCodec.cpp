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

TEST(TestPlantPortCodec, a_window_carries_its_place_and_each_signals_minimum_then_maximum)
{
    Window window{ 7, 1234567890123ull, 10 };
    for (std::size_t signal = 0; signal != signalCount; ++signal)
    {
        window.minimum[signal] = -static_cast<float>(signal);
        window.maximum[signal] = static_cast<float>(signal) + 0.5f;
    }

    const auto bytes = Encode(window);
    EXPECT_EQ(bytes.size(), 3u + 13u + signalCount * 8u);

    const auto decoded = RoundTrip(window);
    EXPECT_EQ(decoded.sequence, 7u);
    EXPECT_EQ(decoded.firstPeriod, 1234567890123ull);
    EXPECT_EQ(decoded.periods, 10);
    EXPECT_FLOAT_EQ(decoded.minimum[24], -24.0f);
    EXPECT_FLOAT_EQ(decoded.maximum[24], 24.5f);

    const std::vector<uint8_t> payload(bytes.begin() + 3, bytes.end());
    PayloadReader reader{ payload };
    reader.U32();
    reader.U64();
    reader.U8();
    EXPECT_FLOAT_EQ(reader.F32(), 0.0f);
    EXPECT_FLOAT_EQ(reader.F32(), 0.5f);
    EXPECT_FLOAT_EQ(reader.F32(), -1.0f);
}

TEST(TestPlantPortCodec, a_stream_request_says_on_or_off)
{
    EXPECT_THAT(Encode(StreamRequest{ 1 }), testing::ElementsAre(0x90, 1, 0, 1));
    EXPECT_EQ(RoundTrip(StreamRequest{ 0 }).on, 0);
}

TEST(TestPlantPortCodec, a_trigger_carries_its_kind_signal_level_and_led_mask)
{
    const auto request = RoundTrip(CaptureRequest{ Trigger{ TriggerKind::rising, 15, 0.75f, led::failure }, 25, 4096 });

    EXPECT_EQ(request.trigger.kind, TriggerKind::rising);
    EXPECT_EQ(request.trigger.signal, 15);
    EXPECT_FLOAT_EQ(request.trigger.level, 0.75f);
    EXPECT_EQ(request.trigger.leds, led::failure);
    EXPECT_EQ(request.preTriggerPercent, 25);
    EXPECT_EQ(request.length, 4096);
}

TEST(TestPlantPortCodec, a_capture_event_names_the_capture_what_happened_and_when)
{
    const auto event = RoundTrip(CaptureEvent{ 9, CaptureEventKind::triggered, 123456789ull });

    EXPECT_EQ(event.capture, 9);
    EXPECT_EQ(event.event, CaptureEventKind::triggered);
    EXPECT_EQ(event.period, 123456789ull);
}

TEST(TestPlantPortCodec, a_capture_chunk_carries_its_place_and_every_signal_of_every_period)
{
    CaptureChunk chunk{ 4, 1300, 325, 1000000ull, 512, {} };
    chunk.periods.resize(3);
    chunk.periods[2][24] = 42.0f;
    chunk.periods[0][0] = -1.5f;

    EXPECT_EQ(Encode(chunk).size(), 3u + 18u + 3u * signalCount * 4u);

    const auto decoded = RoundTrip(chunk);
    EXPECT_EQ(decoded.capture, 4);
    EXPECT_EQ(decoded.length, 1300);
    EXPECT_EQ(decoded.preTrigger, 325);
    EXPECT_EQ(decoded.firstPeriod, 1000000ull);
    EXPECT_EQ(decoded.index, 512);
    ASSERT_EQ(decoded.periods.size(), 3u);
    EXPECT_FLOAT_EQ(decoded.periods[0][0], -1.5f);
    EXPECT_FLOAT_EQ(decoded.periods[2][24], 42.0f);
}

TEST(TestPlantPortCodec, a_profile_carries_its_trigger_and_each_change_with_its_own_payload)
{
    ProfileRequest profile{ Trigger{ TriggerKind::immediately }, {} };
    profile.steps.push_back(ProfileStep{ 0, MessageType::loadTorque, 0.2f, {} });
    profile.steps.push_back(ProfileStep{ 2000, MessageType::externalTorque, 0.0f, ExternalTorque{ TorqueForm::sine, { 0.0f, 0.1f, 5.0f, 0.0f } } });
    profile.steps.push_back(ProfileStep{ 4000, MessageType::encoderNoise, 0.001f, {} });

    const auto decoded = RoundTrip(profile);

    EXPECT_TRUE(decoded.readable);
    ASSERT_EQ(decoded.steps.size(), 3u);
    EXPECT_EQ(decoded.steps[0].change, MessageType::loadTorque);
    EXPECT_FLOAT_EQ(decoded.steps[0].value, 0.2f);
    EXPECT_EQ(decoded.steps[1].at, 2000u);
    EXPECT_EQ(decoded.steps[1].torque.form, TorqueForm::sine);
    EXPECT_FLOAT_EQ(decoded.steps[1].torque.parameters[2], 5.0f);
    EXPECT_FLOAT_EQ(decoded.steps[2].value, 0.001f);
}

TEST(TestPlantPortCodec, a_profile_change_of_another_type_leaves_the_profile_unreadable)
{
    const auto profile = Decode<ProfileRequest>(Frame{ static_cast<uint8_t>(MessageType::profile), { 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0x81, 0, 0 } });

    ASSERT_TRUE(profile.has_value());
    EXPECT_FALSE(profile->readable);
}

TEST(TestPlantPortCodec, a_profile_state_carries_its_progress)
{
    const auto state = RoundTrip(ProfileState{ ProfileStateKind::running, 2, 5, 40000 });

    EXPECT_EQ(state.state, ProfileStateKind::running);
    EXPECT_EQ(state.applied, 2);
    EXPECT_EQ(state.total, 5);
    EXPECT_EQ(state.elapsed, 40000u);
}

TEST(TestPlantPortCodec, cancel_capture_and_stop_profile_carry_nothing)
{
    EXPECT_THAT(Encode(CancelCapture{}), testing::ElementsAre(0x92, 0, 0));
    EXPECT_THAT(Encode(StopProfile{}), testing::ElementsAre(0x94, 0, 0));
}
