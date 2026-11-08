#include "core/services/leds/StatusLeds.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <gtest/gtest.h>
#include <string>

namespace
{
    using state_machine::FaultCode;

    class StatusSourceStub
        : public services::LedStatusSource
    {
    public:
        services::LedStatus Status() const override
        {
            return status;
        }

        services::LedStatus status{ services::LedLifecycle::idle, FaultCode::none };
    };

    class StatusLedsTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        std::string Sample(hal::GpioPinStub& pin, uint32_t ticks)
        {
            std::string levels;
            for (uint32_t tick = 0; tick != ticks; ++tick)
            {
                levels += pin.GetStubState() ? '#' : '.';
                ForwardTime(std::chrono::milliseconds(50));
            }
            return levels;
        }

        hal::GpioPinStub green;
        hal::GpioPinStub amber;
        hal::GpioPinStub red;
        StatusSourceStub source;
        services::StatusLeds leds{ green, amber, red, source };
    };
}

TEST_F(StatusLedsTest, the_leds_are_driven_from_the_moment_the_service_exists)
{
    EXPECT_TRUE(green.GetStubState());
    EXPECT_TRUE(amber.GetStubState());
    EXPECT_FALSE(red.GetStubState());
}

TEST_F(StatusLedsTest, idle_heartbeats_green_and_blinks_amber_slowly)
{
    EXPECT_EQ("#" + std::string(39, '.'), Sample(green, 40));
    EXPECT_EQ(std::string(10, '#') + std::string(10, '.'), Sample(amber, 20));
    EXPECT_EQ(std::string(20, '.'), Sample(red, 20));
}

TEST_F(StatusLedsTest, calibrating_lights_amber_steadily)
{
    source.status.lifecycle = services::LedLifecycle::calibrating;
    ForwardTime(std::chrono::milliseconds(50));

    EXPECT_EQ(std::string(30, '#'), Sample(amber, 30));
}

TEST_F(StatusLedsTest, enabled_lights_green_steadily_and_puts_amber_out)
{
    source.status.lifecycle = services::LedLifecycle::enabled;
    ForwardTime(std::chrono::milliseconds(50));

    EXPECT_EQ(std::string(60, '#'), Sample(green, 60));
    EXPECT_FALSE(amber.GetStubState());
}

TEST_F(StatusLedsTest, a_fault_puts_green_out_and_flashes_the_code_on_red)
{
    source.status = { services::LedLifecycle::fault, FaultCode::overvoltage };
    ForwardTime(std::chrono::milliseconds(50));

    EXPECT_FALSE(green.GetStubState());
    EXPECT_EQ(std::string("#####.....#####.....#####.....") + std::string(30, '.') + "#####", Sample(red, 65));
}

TEST_F(StatusLedsTest, a_new_fault_code_restarts_the_red_pattern)
{
    source.status = { services::LedLifecycle::fault, FaultCode::overcurrent };
    ForwardTime(std::chrono::milliseconds(50));
    Sample(red, 13);

    source.status.fault = FaultCode::encoderLoss;
    ForwardTime(std::chrono::milliseconds(50));

    EXPECT_TRUE(red.GetStubState());
    EXPECT_EQ("#####.....#####.....", Sample(red, 20).substr(0, 20));
}

TEST_F(StatusLedsTest, an_unchanged_pattern_is_not_restarted_by_a_change_of_fault_code_within_its_class)
{
    source.status = { services::LedLifecycle::fault, FaultCode::stall };
    ForwardTime(std::chrono::milliseconds(50));
    Sample(red, 7);

    source.status.fault = FaultCode::overspeed;
    ForwardTime(std::chrono::milliseconds(50));

    EXPECT_FALSE(red.GetStubState());
}

TEST_F(StatusLedsTest, clearing_the_fault_puts_red_out)
{
    source.status = { services::LedLifecycle::fault, FaultCode::hardwareFault };
    ForwardTime(std::chrono::milliseconds(50));
    ASSERT_TRUE(red.GetStubState());

    source.status = { services::LedLifecycle::ready, FaultCode::none };
    ForwardTime(std::chrono::milliseconds(50));

    EXPECT_FALSE(red.GetStubState());
}
