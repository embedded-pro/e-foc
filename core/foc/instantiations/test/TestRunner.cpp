#include "core/foc/instantiations/Runner.hpp"
#include "core/foc/interfaces/test_doubles/FocMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    using testing::_;
    using testing::Return;

    MATCHER_P(DutiesEqual, expected, "")
    {
        return arg.a.Value() == expected.a.Value() && arg.b.Value() == expected.b.Value() && arg.c.Value() == expected.c.Value();
    }

    class TestRunner
        : public testing::Test
    {
    public:
        TestRunner()
        {
            EXPECT_CALL(inverterMock, BaseFrequency())
                .Times(testing::AnyNumber())
                .WillRepeatedly(Return(hal::Hertz{ 20000 }));
            EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _))
                .Times(testing::AnyNumber())
                .WillRepeatedly([this](hal::Hertz, const infra::Function<void(foc::PhaseCurrents)>& onDone)
                    {
                        inverterMock.StorePhaseCurrentsCallback(onDone);
                    });
        }

        testing::StrictMock<drivers::ThreePhaseInverterMock> inverterMock;
        testing::StrictMock<drivers::EncoderMock> encoderMock;
        testing::StrictMock<foc::FocTorqueMock> focMock;
    };
}

TEST_F(TestRunner, ConstructionRegistersPhaseCurrentsCallback)
{
    EXPECT_CALL(inverterMock, PhaseCurrentsReady(hal::Hertz{ 20000 }, _));

    foc::Runner runner{ inverterMock, encoderMock, focMock };

    testing::InSequence seq;
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());
}

TEST_F(TestRunner, EnableStartsFocThenInverter)
{
    foc::Runner runner{ inverterMock, encoderMock, focMock };

    testing::InSequence seq;
    EXPECT_CALL(focMock, Enable());
    EXPECT_CALL(inverterMock, Start());
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());

    runner.Enable();
}

TEST_F(TestRunner, DisableStopsInverterThenFoc)
{
    foc::Runner runner{ inverterMock, encoderMock, focMock };

    testing::InSequence seq;
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());

    runner.Disable();
    // destructor calls Disable() again
}

TEST_F(TestRunner, PhaseCurrentsCallbackReadsEncoderCalculatesFocAndOutputsPwm)
{
    EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _))
        .Times(2)
        .WillRepeatedly([this](hal::Hertz, const infra::Function<void(foc::PhaseCurrents)>& onDone)
            {
                inverterMock.StorePhaseCurrentsCallback(onDone);
            });

    foc::Runner runner{ inverterMock, encoderMock, focMock };

    const foc::PhaseCurrents testCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    const foc::PhasePwmDutyCycles expectedDuties{ hal::Percent{ 60 }, hal::Percent{ 30 }, hal::Percent{ 10 } };

    {
        testing::InSequence seq;
        EXPECT_CALL(focMock, Enable());
        EXPECT_CALL(inverterMock, Start());
        EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.5f }));
        EXPECT_CALL(focMock, Calculate(_, _)).WillOnce(Return(expectedDuties));
        EXPECT_CALL(inverterMock, ThreePhasePwmOutput(DutiesEqual(expectedDuties)));
        EXPECT_CALL(inverterMock, Stop());
        EXPECT_CALL(focMock, Disable());
    }

    runner.Enable();
    inverterMock.TriggerPhaseCurrentsCallback(testCurrents);
}

TEST_F(TestRunner, DestructorCallsDisable)
{
    auto runner = std::make_unique<foc::Runner>(inverterMock, encoderMock, focMock);

    testing::InSequence seq;
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());

    runner.reset();
}

TEST_F(TestRunner, MultipleEnableDisableCyclesWork)
{
    foc::Runner runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(focMock, Enable()).Times(2);
    EXPECT_CALL(inverterMock, Start()).Times(2);
    EXPECT_CALL(inverterMock, Stop()).Times(3); // 2 explicit + 1 destructor
    EXPECT_CALL(focMock, Disable()).Times(3);   // 2 explicit + 1 destructor

    runner.Enable();
    runner.Disable();
    runner.Enable();
    runner.Disable();
    // destructor calls Disable() again
}

TEST_F(TestRunner, ALateCallbackAfterDisableDoesNotDriveThePwm)
{
    EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _))
        .Times(2)
        .WillRepeatedly([this](hal::Hertz, const infra::Function<void(foc::PhaseCurrents)>& onDone)
            {
                inverterMock.StorePhaseCurrentsCallback(onDone);
            });

    foc::Runner runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(focMock, Enable());
    EXPECT_CALL(inverterMock, Start());
    runner.Enable();

    EXPECT_CALL(inverterMock, Stop()).Times(2);
    EXPECT_CALL(focMock, Disable()).Times(2);
    runner.Disable();

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });
}

TEST_F(TestRunner, ACallbackBeforeEnableDoesNotDriveThePwm)
{
    EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](hal::Hertz, const infra::Function<void(foc::PhaseCurrents)>& onDone)
            {
                inverterMock.StorePhaseCurrentsCallback(onDone);
            });

    foc::Runner runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });
}

TEST_F(TestRunner, BalancedCurrentsAreDispatchedToTheControlLaw)
{
    foc::Runner runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(focMock, Enable());
    EXPECT_CALL(inverterMock, Start());
    runner.Enable();

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(_, _)).WillOnce(Return(foc::PhasePwmDutyCycles{ hal::Percent{ 50 }, hal::Percent{ 50 }, hal::Percent{ 50 } }));
    EXPECT_CALL(inverterMock, ThreePhasePwmOutput(_));

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });

    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());
}

TEST_F(TestRunner, ASingleImplausibleSampleIsToleratedAsATransient)
{
    foc::Runner runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(focMock, Enable());
    EXPECT_CALL(inverterMock, Start());
    runner.Enable();

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(_, _)).WillOnce(Return(foc::PhasePwmDutyCycles{ hal::Percent{ 50 }, hal::Percent{ 50 }, hal::Percent{ 50 } }));
    EXPECT_CALL(inverterMock, ThreePhasePwmOutput(_));

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 5.0f }, foc::Ampere{ 5.0f }, foc::Ampere{ 5.0f } });

    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());
}

TEST_F(TestRunner, SustainedKirchhoffViolationDisablesTheDriveAndReports)
{
    foc::Runner runner{ inverterMock, encoderMock, focMock };

    bool reported = false;
    runner.RegisterOnImplausibleCurrents([&reported]()
        {
            reported = true;
        });

    EXPECT_CALL(focMock, Enable());
    EXPECT_CALL(inverterMock, Start());
    runner.Enable();

    EXPECT_CALL(encoderMock, Read()).Times(testing::AnyNumber()).WillRepeatedly(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(_, _)).Times(testing::AnyNumber()).WillRepeatedly(Return(foc::PhasePwmDutyCycles{ hal::Percent{ 50 }, hal::Percent{ 50 }, hal::Percent{ 50 } }));
    EXPECT_CALL(inverterMock, ThreePhasePwmOutput(_)).Times(testing::AnyNumber());
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());

    const foc::PhaseCurrents implausible{ foc::Ampere{ 5.0f }, foc::Ampere{ 5.0f }, foc::Ampere{ 5.0f } };
    for (int sample = 0; sample != 12; ++sample)
        inverterMock.TriggerPhaseCurrentsCallback(implausible);

    EXPECT_TRUE(reported);

    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());
}

TEST_F(TestRunner, ABalancedSampleClearsTheImplausibleRun)
{
    foc::Runner runner{ inverterMock, encoderMock, focMock };

    bool reported = false;
    runner.RegisterOnImplausibleCurrents([&reported]()
        {
            reported = true;
        });

    EXPECT_CALL(focMock, Enable());
    EXPECT_CALL(inverterMock, Start());
    runner.Enable();

    EXPECT_CALL(encoderMock, Read()).Times(testing::AnyNumber()).WillRepeatedly(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(_, _)).Times(testing::AnyNumber()).WillRepeatedly(Return(foc::PhasePwmDutyCycles{ hal::Percent{ 50 }, hal::Percent{ 50 }, hal::Percent{ 50 } }));
    EXPECT_CALL(inverterMock, ThreePhasePwmOutput(_)).Times(testing::AnyNumber());

    const foc::PhaseCurrents implausible{ foc::Ampere{ 5.0f }, foc::Ampere{ 5.0f }, foc::Ampere{ 5.0f } };
    const foc::PhaseCurrents balanced{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };

    for (int cycle = 0; cycle != 4; ++cycle)
    {
        for (int sample = 0; sample != 6; ++sample)
            inverterMock.TriggerPhaseCurrentsCallback(implausible);

        inverterMock.TriggerPhaseCurrentsCallback(balanced);
    }

    EXPECT_FALSE(reported);

    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());
}
