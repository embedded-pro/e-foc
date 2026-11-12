#include "core/foc/model/drive/BoardProtectionSimulator.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using Reason = application::PlatformFactory::BoardProtectionReason;
    using State = application::PlatformFactory::BoardProtectionState;

    foc::PhaseCurrents Currents(float a, float b, float c)
    {
        return { foc::Ampere{ a }, foc::Ampere{ b }, foc::Ampere{ c } };
    }

    class TestBoardProtectionSimulator
        : public testing::Test
    {
    public:
        TestBoardProtectionSimulator()
        {
            protection.Configure({ .overCurrentAmpere = 10.0f, .overVoltageVolts = 30.0f, .underVoltageVolts = 12.0f, .overTemperatureCelsius = 120.0f });
            protection.Register([this](Reason reason)
                {
                    tripped.Call(reason);
                });
            protection.SetArmed(true);
        }

        application::BoardProtectionSimulator protection;
        testing::StrictMock<testing::MockFunction<void(Reason)>> tripped;
    };
}

TEST_F(TestBoardProtectionSimulator, a_peak_phase_current_above_the_trip_is_delivered_once)
{
    protection.Evaluate(Currents(1.0f, -11.0f, 10.0f), 24.0f, 25.0f);
    EXPECT_EQ(protection.Status(), State::asserted);

    EXPECT_CALL(tripped, Call(Reason::overCurrent));
    protection.DeliverPendingProtection();

    protection.Evaluate(Currents(1.0f, -11.0f, 10.0f), 24.0f, 25.0f);
    protection.DeliverPendingProtection();
}

TEST_F(TestBoardProtectionSimulator, each_supply_and_thermal_trip_names_its_reason)
{
    testing::InSequence sequence;
    EXPECT_CALL(tripped, Call(Reason::overVoltage));
    EXPECT_CALL(tripped, Call(Reason::underVoltage));
    EXPECT_CALL(tripped, Call(Reason::overTemperature));

    protection.Evaluate(Currents(0.0f, 0.0f, 0.0f), 31.0f, 25.0f);
    protection.DeliverPendingProtection();
    protection.Evaluate(Currents(0.0f, 0.0f, 0.0f), 24.0f, 25.0f);
    protection.Evaluate(Currents(0.0f, 0.0f, 0.0f), 11.0f, 25.0f);
    protection.DeliverPendingProtection();
    protection.Evaluate(Currents(0.0f, 0.0f, 0.0f), 24.0f, 25.0f);
    protection.Evaluate(Currents(0.0f, 0.0f, 0.0f), 24.0f, 121.0f);
    protection.DeliverPendingProtection();
}

TEST_F(TestBoardProtectionSimulator, a_disarmed_board_trips_nothing)
{
    protection.SetArmed(false);

    protection.Evaluate(Currents(50.0f, 0.0f, 0.0f), 50.0f, 200.0f);
    protection.DeliverPendingProtection();

    EXPECT_EQ(protection.Status(), State::clear);
}

TEST_F(TestBoardProtectionSimulator, a_zero_trip_level_is_off)
{
    protection.Configure({});

    protection.Evaluate(Currents(50.0f, 0.0f, 0.0f), 0.5f, 200.0f);
    protection.DeliverPendingProtection();

    EXPECT_EQ(protection.Status(), State::clear);
}
