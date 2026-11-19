#include "core/foc/model/drive/SilPlantModel.hpp"
#include "infra/util/StaticStorage.hpp"
#include "motor_parameters/PlantPresets.hpp"
#include "tools/virtual_motor/platform/PlatformPortTarget.hpp"
#include "gtest/gtest.h"
#include <cmath>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace
{
    using namespace virtual_motor::port;

    class Reports
        : public virtual_motor::InstrumentReports
    {
    public:
        void ReportWindow(const Window& window) override
        {
            windows.push_back(window);
        }

        void ReportCaptureEvent(const CaptureEvent& event) override
        {
            events.push_back(event);
        }

        void ReportCaptureChunk(const CaptureChunk& chunk) override
        {
            chunks.push_back(chunk);
        }

        void ReportProfileState(const ProfileState& state) override
        {
            states.push_back(state);
        }

        std::vector<Window> windows;
        std::vector<CaptureEvent> events;
        std::vector<CaptureChunk> chunks;
        std::vector<ProfileState> states;
    };

    class TestPlatformPortTarget
        : public testing::Test
    {
    public:
        static void SetUpTestSuite()
        {
            platform.Construct(virtual_motor::BoardOptions{ "bench", 4, sil::NominalPlant(), MemoryFile(), 1, false, {}, "teknic" });
            target.Construct(*platform);
        }

        static void TearDownTestSuite()
        {
            std::filesystem::remove(MemoryFile());
        }

        void TearDown() override
        {
            target->Resume();
            target->ClearAll();
            platform->OperationalLed().Set(false);
            platform->FailureLed().Set(false);
        }

        static virtual_motor::ShaftDrive Drive(float speed, uint64_t period, float torque, float inertia)
        {
            return { foc::RadiansPerSecond{ speed }, period, foc::NewtonMeter{ torque }, foc::KilogramMeterSquared{ inertia } };
        }

        static uint64_t Now()
        {
            return target->CurrentStatus().controlPeriods;
        }

        static float Signal(const Sample& sample, virtual_motor::Signal signal)
        {
            return sample[static_cast<std::size_t>(signal)];
        }

        static float RotorSpeedAfterOnePeriod()
        {
            target->Step(StepUnit::controlPeriod, 1);
            return Signal(platform->SamplePlant(), virtual_motor::Signal::rotorSpeed);
        }

        static std::string MemoryFile()
        {
            return (std::filesystem::temp_directory_path() / "e-foc-virtual-motor-board-test.nvm").string();
        }

        static constexpr float heavyInertia{ 1.0e3f };
        static constexpr float armInertia{ 1.0e-4f };
        static infra::StaticStorage<virtual_motor::VirtualPlatform> platform;
        static infra::StaticStorage<virtual_motor::PlatformPortTarget> target;
    };

    infra::StaticStorage<virtual_motor::VirtualPlatform> TestPlatformPortTarget::platform;
    infra::StaticStorage<virtual_motor::PlatformPortTarget> TestPlatformPortTarget::target;
}

TEST_F(TestPlatformPortTarget, introduces_the_board_by_bus_node_plant_and_control_rate)
{
    const auto hello = target->Identity();

    EXPECT_EQ(hello.version, protocolVersion);
    EXPECT_EQ(hello.node, 4);
    EXPECT_EQ(hello.bus, "bench");
    EXPECT_EQ(hello.plant, "teknic");
    EXPECT_EQ(hello.controlRateHz, platform->BaseFrequency().Value());
}

TEST_F(TestPlatformPortTarget, a_paused_board_steps_by_control_periods_and_milliseconds)
{
    target->Pause();
    EXPECT_EQ(target->CurrentStatus().runState, RunState::paused);
    const auto firstStep = target->CurrentStatus().controlPeriods;

    EXPECT_FALSE(target->Step(StepUnit::controlPeriod, 3).has_value());
    EXPECT_EQ(target->CurrentStatus().controlPeriods, firstStep + 3);

    EXPECT_FALSE(target->Step(StepUnit::millisecond, 1).has_value());
    EXPECT_EQ(target->CurrentStatus().controlPeriods, firstStep + platform->BaseFrequency().Value() / 1000);

    target->Resume();
    EXPECT_EQ(target->CurrentStatus().runState, RunState::running);
    EXPECT_EQ(target->Step(StepUnit::controlPeriod, 1), RefusalReason::stepWhileRunning);
}

TEST_F(TestPlatformPortTarget, the_status_shows_the_board_leds)
{
    platform->OperationalLed().Set(true);
    platform->FailureLed().Set(true);

    EXPECT_EQ(target->CurrentStatus().leds, led::operational | led::failure);
}

TEST_F(TestPlatformPortTarget, changes_reach_the_plant_and_are_listed_until_cleared)
{
    EXPECT_FALSE(target->SetLoadTorque(0.05f).has_value());
    EXPECT_FALSE(target->SetExternalTorque(ExternalTorque{ TorqueForm::constant, { 0.1f } }).has_value());
    EXPECT_FALSE(target->SetCurrentNoise(0.03f).has_value());
    EXPECT_FALSE(target->SetEncoderNoise(0.002f).has_value());
    EXPECT_EQ(target->SetCurrentNoise(-1.0f), RefusalReason::outOfRange);

    const auto changes = target->ChangesInForce();
    EXPECT_FLOAT_EQ(changes.loadTorque, 0.05f);
    EXPECT_EQ(changes.differs, differs::loadTorque | differs::externalTorque | differs::currentNoise | differs::encoderNoise);

    target->ClearAll();
    EXPECT_EQ(target->ChangesInForce().differs, 0);
}

TEST_F(TestPlatformPortTarget, a_driven_shaft_turns_at_the_speed_sent_from_the_next_control_period)
{
    Reports reports;
    target->Attach(reports);
    target->Pause();
    EXPECT_FALSE(target->SetShaftSpeed(Drive(5.0f, Now(), 0.0f, heavyInertia)).has_value());
    EXPECT_EQ(target->SetShaftSpeed(Drive(std::numeric_limits<float>::quiet_NaN(), Now(), 0.0f, heavyInertia)), RefusalReason::outOfRange);
    EXPECT_EQ(target->SetShaftSpeed(Drive(5.0f, Now(), 0.0f, 0.0f)), RefusalReason::outOfRange);
    EXPECT_TRUE(target->ChangesInForce().shaftDriven);

    target->Stream(true);
    target->Step(StepUnit::controlPeriod, 10);
    target->Stream(false);
    target->Detach();

    ASSERT_EQ(reports.windows.size(), 1u);
    const auto rotorSpeed = static_cast<std::size_t>(virtual_motor::Signal::rotorSpeed);
    EXPECT_NEAR(reports.windows[0].minimum[rotorSpeed], 5.0f, 1e-4f);
    EXPECT_NEAR(reports.windows[0].maximum[rotorSpeed], 5.0f, 1e-4f);

    target->SetShaftSpeed(Drive(0.0f, Now(), 0.0f, heavyInertia));
    target->Step(StepUnit::controlPeriod, 1);
    target->ClearAll();
    EXPECT_FALSE(target->ChangesInForce().shaftDriven);
}

TEST_F(TestPlatformPortTarget, a_driven_shaft_adds_the_electrical_torque_it_was_not_sent_since_the_period_used)
{
    Reports reports;
    target->Attach(reports);
    target->Pause();
    target->SetShaftSpeed(Drive(20.0f, Now(), 0.0f, heavyInertia));
    const auto first = Now();
    target->Capture(CaptureRequest{ Trigger{ TriggerKind::immediately }, 0, 40 });
    target->Step(StepUnit::controlPeriod, 40);
    target->Detach();
    ASSERT_EQ(reports.chunks.size(), 1u);
    ASSERT_EQ(reports.chunks[0].firstPeriod, first);
    ASSERT_EQ(reports.chunks[0].periods.size(), 40u);

    const auto now = Now();
    const auto used = now - 31;
    const auto periodSeconds = 1.0f / static_cast<float>(platform->BaseFrequency().Value());
    float unseenImpulse{ 0.0f };
    for (auto period = used + 1; period != now; ++period)
        unseenImpulse += Signal(reports.chunks[0].periods[period - first], virtual_motor::Signal::electricalTorque) * periodSeconds;
    const auto expected = 20.0f + unseenImpulse / armInertia;
    ASSERT_GT(std::abs(expected - 20.0f), 0.1f);

    target->SetShaftSpeed(Drive(20.0f, used, 0.0f, armInertia));

    EXPECT_NEAR(RotorSpeedAfterOnePeriod(), expected, 1e-4f);
}

TEST_F(TestPlatformPortTarget, a_driven_shaft_sent_the_electrical_torque_it_has_holds_the_speed_sent)
{
    Reports reports;
    target->Attach(reports);
    target->Pause();
    target->SetShaftSpeed(Drive(20.0f, Now(), 0.0f, heavyInertia));
    target->Step(StepUnit::controlPeriod, 400);
    target->Stream(true);
    target->Step(StepUnit::controlPeriod, 10);
    target->Stream(false);
    target->Step(StepUnit::controlPeriod, 30);
    target->Detach();
    ASSERT_EQ(reports.windows.size(), 1u);

    const auto& window = reports.windows[0];
    const auto used = window.firstPeriod + window.periods - 1;
    const auto torque = Signal(window.last, virtual_motor::Signal::electricalTorque);
    ASSERT_GT(std::abs(torque), 0.01f);

    target->SetShaftSpeed(Drive(20.0f, used, torque, armInertia));

    EXPECT_NEAR(RotorSpeedAfterOnePeriod(), 20.0f, 1e-3f);
}

TEST_F(TestPlatformPortTarget, a_stream_closes_a_window_every_ten_control_periods)
{
    Reports reports;
    target->Attach(reports);
    target->Pause();
    const auto first = target->CurrentStatus().controlPeriods;
    target->Stream(true);
    target->Step(StepUnit::controlPeriod, 25);
    target->Stream(false);
    target->Step(StepUnit::controlPeriod, 10);
    target->Detach();

    ASSERT_EQ(reports.windows.size(), 2u);
    EXPECT_EQ(reports.windows[0].sequence, 0u);
    EXPECT_EQ(reports.windows[0].firstPeriod, first);
    EXPECT_EQ(reports.windows[1].sequence, 1u);
    EXPECT_EQ(reports.windows[1].firstPeriod, first + 10);
    EXPECT_EQ(reports.windows[1].periods, 10);
    const auto busVoltage = static_cast<std::size_t>(virtual_motor::Signal::busVoltage);
    EXPECT_GT(reports.windows[0].minimum[busVoltage], 0.0f);
    EXPECT_LE(reports.windows[0].minimum[busVoltage], reports.windows[0].maximum[busVoltage]);
}

TEST_F(TestPlatformPortTarget, an_immediate_capture_records_the_periods_that_follow_it)
{
    Reports reports;
    target->Attach(reports);
    target->Pause();
    const auto first = target->CurrentStatus().controlPeriods;

    EXPECT_FALSE(target->Capture(CaptureRequest{ Trigger{ TriggerKind::immediately }, 0, 600 }).has_value());
    target->Step(StepUnit::controlPeriod, 700);
    target->Detach();

    ASSERT_EQ(reports.events.size(), 2u);
    EXPECT_EQ(reports.events[1].event, CaptureEventKind::triggered);
    ASSERT_EQ(reports.chunks.size(), 2u);
    EXPECT_EQ(reports.chunks[0].firstPeriod, first);
    EXPECT_EQ(reports.chunks[1].index, 512);
    EXPECT_EQ(reports.chunks[1].periods.size(), 88u);
}

TEST_F(TestPlatformPortTarget, a_profile_changes_the_plant_in_the_period_its_time_names)
{
    Reports reports;
    target->Attach(reports);
    target->Pause();

    EXPECT_FALSE(target->RunProfile(ProfileRequest{ Trigger{ TriggerKind::immediately }, { ProfileStep{ 20, MessageType::loadTorque, 0.05f, {} } } }).has_value());
    target->Step(StepUnit::controlPeriod, 20);
    EXPECT_EQ(target->ChangesInForce().differs, 0);

    target->Step(StepUnit::controlPeriod, 1);
    EXPECT_FLOAT_EQ(target->ChangesInForce().loadTorque, 0.05f);
    EXPECT_EQ(reports.states.back().state, ProfileStateKind::finished);
    target->Detach();
}
