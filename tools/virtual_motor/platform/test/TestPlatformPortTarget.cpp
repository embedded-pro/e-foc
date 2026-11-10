#include "core/foc/model/drive/SilPlantModel.hpp"
#include "infra/util/StaticStorage.hpp"
#include "motor_parameters/PlantPresets.hpp"
#include "tools/virtual_motor/platform/PlatformPortTarget.hpp"
#include "gtest/gtest.h"
#include <filesystem>
#include <string>

namespace
{
    using namespace virtual_motor::port;

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

        static std::string MemoryFile()
        {
            return (std::filesystem::temp_directory_path() / "e-foc-virtual-motor-board-test.nvm").string();
        }

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
