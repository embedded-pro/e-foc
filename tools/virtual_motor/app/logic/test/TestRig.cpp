#include "can-lite/drivers/implementation/LoopbackSocket.hpp"
#include "tools/virtual_motor/app/logic/Rig.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>
#include <filesystem>
#include <thread>

namespace
{
    using namespace virtual_motor::app;
    namespace port = virtual_motor::port;
    namespace loopback = services::loopback;

    class FakeProcesses
        : public EngineProcesses
    {
    public:
        std::optional<ProcessId> Start(const std::vector<std::string>& arguments, const std::string& workingDirectory) override
        {
            started.push_back(arguments);
            folders.push_back(workingDirectory);
            return refuse ? std::nullopt : std::optional<ProcessId>{ ++next };
        }

        void Stop(ProcessId process) override
        {
            stopped.push_back(process);
        }

        std::vector<std::vector<std::string>> started;
        std::vector<std::string> folders;
        std::vector<ProcessId> stopped;
        ProcessId next{ 0 };
        bool refuse{ false };
    };

    class FakeStore
        : public RigStore
    {
    public:
        std::vector<MotorSpec> Load() override
        {
            return stored;
        }

        void Save(const std::vector<MotorSpec>& motors) override
        {
            stored = motors;
        }

        std::vector<MotorSpec> stored;
    };

    class RecordingObserver
        : public RigObserver
    {
    public:
        void MotorsChanged() override
        {
            ++listChanges;
        }

        void MotorUpdated(uint8_t) override
        {
            ++updates;
        }

        void MotorLogged(uint8_t, const std::string& line) override
        {
            lines.push_back(line);
        }

        void MotorRefused(uint8_t, const port::Refused& refused) override
        {
            refusals.push_back(refused);
        }

        void MotorCaptured(uint8_t) override
        {
            ++captures;
        }

        int listChanges{ 0 };
        int updates{ 0 };
        int captures{ 0 };
        std::vector<std::string> lines;
        std::vector<port::Refused> refusals;
    };

    class FakeEngine
    {
    public:
        explicit FakeEngine(uint16_t port)
            : socket(loopback::Connect(port))
        {}

        ~FakeEngine()
        {
            loopback::Close(socket);
        }

        template<class Message>
        void Send(const Message& message)
        {
            const auto bytes = port::Encode(message);
            loopback::Send(socket, infra::ConstByteRange{ bytes.data(), bytes.data() + bytes.size() });
        }

        std::vector<port::Frame> Received()
        {
            std::vector<port::Frame> frames;
            std::array<uint8_t, 4096> buffer{};
            for (int attempt = 0; attempt != 50; ++attempt)
            {
                if (auto size = loopback::Receive(socket, infra::ByteRange{ buffer.data(), buffer.data() + buffer.size() }); size.value_or(0) != 0)
                    assembler.Append(buffer.data(), *size);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            while (auto frame = assembler.Next())
                frames.push_back(*frame);
            return frames;
        }

        void Close()
        {
            loopback::Close(socket);
            socket = loopback::invalidSocket;
        }

        intptr_t socket;
        port::FrameAssembler assembler;
    };

    class TestRig
        : public testing::Test
    {
    public:
        TestRig()
        {
            rig.Observe(observer);
            EXPECT_TRUE(rig.Open());
        }

        ~TestRig() override
        {
            std::filesystem::remove_all(folder);
        }

        void PollFor(int milliseconds)
        {
            for (int millisecond = 0; millisecond != milliseconds; ++millisecond)
            {
                rig.Poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        std::unique_ptr<FakeEngine> Introduce(uint8_t node, port::RunState state = port::RunState::running)
        {
            auto engine = std::make_unique<FakeEngine>(rig.PlantPort().value_or(0));
            engine->Send(port::Hello{ port::protocolVersion, node, "default", "teknic", 20000 });
            engine->Send(port::Changes{});
            engine->Send(port::Status{ 100, 1.0f, 0, state, 0, 0 });
            PollFor(30);
            return engine;
        }

        std::string folder{ (std::filesystem::temp_directory_path() / "e-foc-rig-test").string() };
        FakeProcesses processes;
        FakeStore store;
        RecordingObserver observer;
        Rig rig{ processes, store, folder, 29999 };
    };
}

TEST_F(TestRig, adding_a_motor_starts_its_engine_in_its_own_folder_on_the_rigs_ports)
{
    EXPECT_FALSE(rig.Add(MotorSpec{ "default", 3, "anaheim", StartMode::factoryFresh }).has_value());

    ASSERT_EQ(processes.started.size(), 1u);
    EXPECT_THAT(processes.started[0], testing::ElementsAre("--bus", "default", "--node", "3", "--plant", "anaheim", "--plant-port", std::to_string(*rig.PlantPort()), "--hub-port", "29999", "--factory-fresh"));
    EXPECT_EQ(std::filesystem::path{ processes.folders[0] }.filename(), "default-3");
    EXPECT_TRUE(std::filesystem::is_directory(processes.folders[0]));
    EXPECT_EQ(rig.Find(3)->State(), MotorState::starting);
}

TEST_F(TestRig, a_node_already_in_the_list_is_refused_with_the_reason)
{
    rig.Add(MotorSpec{ "default", 3 });

    const auto refusal = rig.Add(MotorSpec{ "default", 3 });

    ASSERT_TRUE(refusal.has_value());
    EXPECT_THAT(*refusal, testing::HasSubstr("Node 3 is already in the list"));
    EXPECT_EQ(processes.started.size(), 1u);
}

TEST_F(TestRig, the_list_is_kept_and_restored_motors_start_keeping_their_memory)
{
    rig.Add(MotorSpec{ "default", 1, "teknic", StartMode::factoryFresh });
    ASSERT_EQ(store.stored.size(), 1u);
    EXPECT_EQ(store.stored[0].start, StartMode::keepMemory);

    FakeProcesses again;
    Rig restored{ again, store, folder, 29999 };
    EXPECT_TRUE(restored.Open());

    ASSERT_EQ(again.started.size(), 1u);
    EXPECT_THAT(again.started[0], testing::Not(testing::Contains("--factory-fresh")));
    EXPECT_NE(restored.Find(1), nullptr);
}

TEST_F(TestRig, a_motor_runs_once_it_introduces_itself_and_its_log_and_status_follow)
{
    rig.Add(MotorSpec{ "default", 2 });
    auto engine = Introduce(2);

    EXPECT_EQ(rig.Find(2)->State(), MotorState::running);
    ASSERT_TRUE(rig.Find(2)->Identity().has_value());
    EXPECT_EQ(rig.Find(2)->Identity()->controlRateHz, 20000u);
    ASSERT_TRUE(rig.Find(2)->Status().has_value());
    EXPECT_EQ(rig.Find(2)->Status()->controlPeriods, 100u);

    engine->Send(port::LogLine{ "fsm: handled Start" });
    engine->Send(port::Status{ 200, 1.0f, 0, port::RunState::paused, 0, 0 });
    PollFor(30);

    EXPECT_EQ(rig.Find(2)->State(), MotorState::paused);
    EXPECT_THAT(rig.Find(2)->Log(), testing::Contains("fsm: handled Start"));
    EXPECT_THAT(observer.lines, testing::Contains("fsm: handled Start"));
}

TEST_F(TestRig, requests_reach_the_motor_and_refusals_come_back)
{
    rig.Add(MotorSpec{ "default", 2 });
    auto engine = Introduce(2);

    rig.Find(2)->Send(port::LoadTorque{ 0.1f });
    const auto frames = engine->Received();
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_FLOAT_EQ(port::Decode<port::LoadTorque>(frames[0])->newtonMeter, 0.1f);

    engine->Send(port::Refused{ static_cast<uint8_t>(port::MessageType::currentNoise), port::RefusalReason::outOfRange });
    PollFor(30);
    ASSERT_EQ(observer.refusals.size(), 1u);
}

TEST_F(TestRig, the_disturbance_count_follows_the_changes_in_force)
{
    rig.Add(MotorSpec{ "default", 2 });
    auto engine = Introduce(2);

    engine->Send(port::Changes{ 0.1f, {}, 0.0f, 0.0f, port::differs::loadTorque | port::differs::encoderNoise });
    PollFor(30);

    EXPECT_EQ(rig.Find(2)->ActiveDisturbances(), 2);
}

TEST_F(TestRig, a_motor_whose_port_closes_while_its_process_runs_is_restarting)
{
    rig.Add(MotorSpec{ "default", 2 });
    auto engine = Introduce(2);

    engine->Close();
    PollFor(30);
    EXPECT_EQ(rig.Find(2)->State(), MotorState::restarting);

    auto restarted = Introduce(2);
    EXPECT_EQ(rig.Find(2)->State(), MotorState::running);
}

TEST_F(TestRig, a_process_that_ends_unasked_has_crashed_and_one_powered_off_is_stopped)
{
    rig.Add(MotorSpec{ "default", 1 });
    rig.Add(MotorSpec{ "default", 2 });
    rig.ProcessOutput(1, "e-foc-virtual-motor: no plant port is listening");

    rig.ProcessEnded(1);
    rig.PowerOff(2);
    rig.ProcessEnded(2);

    EXPECT_EQ(rig.Find(1)->State(), MotorState::crashed);
    EXPECT_THAT(rig.Find(1)->Log(), testing::Contains("e-foc-virtual-motor: no plant port is listening"));
    EXPECT_EQ(rig.Find(2)->State(), MotorState::stopped);
    EXPECT_FALSE(rig.Find(2)->Status().has_value());
    EXPECT_THAT(processes.stopped, testing::ElementsAre(2u));
}

TEST_F(TestRig, power_cycling_starts_a_new_process_and_the_old_ones_end_is_ignored)
{
    rig.Add(MotorSpec{ "default", 1, "teknic", StartMode::factoryFresh });

    rig.PowerCycle(1);
    rig.ProcessEnded(1);

    EXPECT_THAT(processes.stopped, testing::ElementsAre(1u));
    ASSERT_EQ(processes.started.size(), 2u);
    EXPECT_THAT(processes.started[1], testing::Not(testing::Contains("--factory-fresh")));
    EXPECT_EQ(rig.Find(1)->State(), MotorState::starting);
}

TEST_F(TestRig, a_program_that_cannot_start_is_reported)
{
    processes.refuse = true;

    rig.Add(MotorSpec{ "default", 1 });

    EXPECT_EQ(rig.Find(1)->State(), MotorState::crashed);
    EXPECT_THAT(rig.Find(1)->Log(), testing::Contains(testing::HasSubstr("could not be started")));
}

TEST_F(TestRig, removing_a_motor_powers_it_off_and_forgets_it)
{
    rig.Add(MotorSpec{ "default", 1 });

    rig.Remove(1);

    EXPECT_EQ(rig.Find(1), nullptr);
    EXPECT_TRUE(store.stored.empty());
    EXPECT_THAT(processes.stopped, testing::ElementsAre(1u));
}

TEST_F(TestRig, the_stream_is_asked_for_once_and_again_after_the_motor_restarts)
{
    rig.Add(MotorSpec{ "default", 2 });
    auto engine = Introduce(2);

    rig.Find(2)->Stream(true);
    rig.Find(2)->Stream(true);
    auto frames = engine->Received();
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(port::Decode<port::StreamRequest>(frames[0])->on, 1);

    engine->Send(port::Window{ 0, 0, 10, {}, {} });
    PollFor(30);
    EXPECT_EQ(rig.Find(2)->History().Count(), 1u);

    engine->Close();
    PollFor(30);
    auto restarted = Introduce(2);
    EXPECT_EQ(rig.Find(2)->History().Count(), 0u);
    frames = restarted->Received();
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(port::Decode<port::StreamRequest>(frames[0])->on, 1);

    rig.Find(2)->Stream(false);
    frames = restarted->Received();
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(port::Decode<port::StreamRequest>(frames[0])->on, 0);
}

TEST_F(TestRig, a_capture_is_assembled_and_a_restart_cancels_one_in_flight)
{
    rig.Add(MotorSpec{ "default", 2 });
    auto engine = Introduce(2);

    engine->Send(port::CaptureEvent{ 1, port::CaptureEventKind::armed, 100 });
    engine->Send(port::CaptureEvent{ 1, port::CaptureEventKind::triggered, 110 });
    engine->Send(port::CaptureChunk{ 1, 2, 1, 109, 0, { port::Sample{}, port::Sample{} } });
    PollFor(30);
    EXPECT_EQ(rig.Find(2)->Captures().State(), CaptureState::complete);
    EXPECT_EQ(observer.captures, 3);

    engine->Send(port::CaptureEvent{ 2, port::CaptureEventKind::armed, 200 });
    PollFor(30);
    engine->Close();
    PollFor(30);
    EXPECT_EQ(rig.Find(2)->Captures().State(), CaptureState::cancelled);
    EXPECT_EQ(rig.Find(2)->Captures().Last()->number, 1u);
    EXPECT_EQ(observer.captures, 5);
}
