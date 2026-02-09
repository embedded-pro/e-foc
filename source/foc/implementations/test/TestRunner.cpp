#include "source/foc/implementations/Runner.hpp"
#include "source/foc/implementations/test_doubles/DriversMock.hpp"
#include "source/foc/implementations/test_doubles/FocMock.hpp"
#include "gmock/gmock.h"

namespace
{
    class TestRunner
        : public ::testing::Test
    {
    public:
        TestRunner()
        {
            EXPECT_CALL(driverMock, BaseFrequency())
                .WillOnce(::testing::Return(hal::Hertz{ 10000 }));
            EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_))
                .WillOnce([this](auto, const auto& onDone)
                    {
                        driverMock.StorePhaseCurrentsCallback(onDone);
                    });

            runner.emplace(driverMock, encoderMock, focMock);
        }

        ::testing::StrictMock<foc::FieldOrientedControllerInterfaceMock> driverMock;
        ::testing::StrictMock<foc::EncoderMock> encoderMock;
        ::testing::StrictMock<foc::FocTorqueMock> focMock;
        std::optional<foc::Runner> runner;

        void DestroyRunner()
        {
            EXPECT_CALL(driverMock, Stop());
            EXPECT_CALL(focMock, Disable());
            runner.reset();
        }

        ~TestRunner() override
        {
            if (runner.has_value())
                DestroyRunner();
        }
    };
}

TEST_F(TestRunner, reads_encoder_and_calculates_on_phase_currents_callback)
{
    foc::PhaseCurrents inputCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ 2.0f }, foc::Ampere{ 3.0f } };
    foc::Radians position{ 0.5f };
    foc::PhasePwmDutyCycles expectedDuties{ hal::Percent{ 50 }, hal::Percent{ 60 }, hal::Percent{ 70 } };

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(::testing::Return(position));
    EXPECT_CALL(focMock, Calculate(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(expectedDuties));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(::testing::_));

    driverMock.TriggerPhaseCurrentsCallback(inputCurrents);
}

TEST_F(TestRunner, outputs_duty_cycles_from_foc_calculation)
{
    foc::PhasePwmDutyCycles expectedDuties{ hal::Percent{ 25 }, hal::Percent{ 50 }, hal::Percent{ 75 } };

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(expectedDuties));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(::testing::_))
        .WillOnce([&](const foc::PhasePwmDutyCycles& duties)
            {
                EXPECT_EQ(duties.a, expectedDuties.a);
                EXPECT_EQ(duties.b, expectedDuties.b);
                EXPECT_EQ(duties.c, expectedDuties.c);
            });

    driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
}

TEST_F(TestRunner, enable_enables_foc_and_starts_driver)
{
    ::testing::InSequence seq;
    EXPECT_CALL(focMock, Enable());
    EXPECT_CALL(driverMock, Start());

    runner->Enable();
}

TEST_F(TestRunner, disable_stops_driver_and_disables_foc)
{
    ::testing::InSequence seq;
    EXPECT_CALL(driverMock, Stop());
    EXPECT_CALL(focMock, Disable());

    runner->Disable();
}

TEST_F(TestRunner, destructor_disables)
{
    DestroyRunner();
}
