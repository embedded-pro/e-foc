#include "core/foc/instantiations/Runner.hpp"
#include "core/foc/interfaces/test_doubles/FocMock.hpp"
#include "core/foc/math/DutyConversion.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>

namespace
{
    using testing::_;
    using testing::Return;

    float DutyPercent(hal::DutyCycle duty)
    {
        return 100.0f * foc::DutyFraction(duty);
    }

    MATCHER_P(DutiesEqual, expected, "")
    {
        return DutyPercent(arg.a) == DutyPercent(expected.a) && DutyPercent(arg.b) == DutyPercent(expected.b) && DutyPercent(arg.c) == DutyPercent(expected.c);
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
    EXPECT_CALL(inverterMock, PhaseCurrentsReady(hal::Hertz{ 20000 }, _)).Times(2);

    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

    testing::InSequence seq;
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());
}

TEST_F(TestRunner, EnableStartsFocThenInverter)
{
    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

    testing::InSequence seq;
    EXPECT_CALL(focMock, Enable());
    EXPECT_CALL(inverterMock, Start());
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());

    runner.Enable();
}

TEST_F(TestRunner, DisableStopsInverterThenFoc)
{
    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

    testing::InSequence seq;
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());

    runner.Disable();
}

TEST_F(TestRunner, PhaseCurrentsCallbackReadsEncoderCalculatesFocAndOutputsPwm)
{
    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

    const foc::PhaseCurrents testCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    const foc::PhasePwmDutyCycles expectedDuties{ hal::DutyCycle::FromPercent(60), hal::DutyCycle::FromPercent(30), hal::DutyCycle::FromPercent(10) };

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
    auto runner = std::make_unique<foc::Runner<foc::FocTorqueMock>>(inverterMock, encoderMock, focMock);

    testing::InSequence seq;
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());

    runner.reset();
}

TEST_F(TestRunner, MultipleEnableDisableCyclesWork)
{
    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(focMock, Enable()).Times(2);
    EXPECT_CALL(inverterMock, Start()).Times(2);
    EXPECT_CALL(inverterMock, Stop()).Times(3);
    EXPECT_CALL(focMock, Disable()).Times(3);

    runner.Enable();
    runner.Disable();
    runner.Enable();
    runner.Disable();
}

TEST_F(TestRunner, AStopWhileThePhaseCurrentsSlotIsTakenNeverStartsTheInverter)
{
    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _))
        .WillOnce([this, &runner](hal::Hertz, const infra::Function<void(foc::PhaseCurrents)>& onDone)
            {
                inverterMock.StorePhaseCurrentsCallback(onDone);
                runner.Disable();
            })
        .WillRepeatedly([this](hal::Hertz, const infra::Function<void(foc::PhaseCurrents)>& onDone)
            {
                inverterMock.StorePhaseCurrentsCallback(onDone);
            });

    EXPECT_CALL(inverterMock, Stop()).Times(3);
    EXPECT_CALL(focMock, Disable()).Times(3);

    runner.Enable();

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });
}

TEST_F(TestRunner, AStopWhileTheControlLawIsEnabledNeverStartsTheInverter)
{
    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(focMock, Enable())
        .WillOnce([&runner]()
            {
                runner.Disable();
            });

    EXPECT_CALL(inverterMock, Stop()).Times(3);
    EXPECT_CALL(focMock, Disable()).Times(3);

    runner.Enable();

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });
}

TEST_F(TestRunner, AStopWhileTheInverterStartsLeavesTheBridgeStopped)
{
    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(focMock, Enable());
    EXPECT_CALL(inverterMock, Start())
        .WillOnce([&runner]()
            {
                runner.Disable();
            });

    EXPECT_CALL(inverterMock, Stop()).Times(3);
    EXPECT_CALL(focMock, Disable()).Times(3);

    runner.Enable();

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });
}

TEST_F(TestRunner, ALateCallbackAfterDisableDoesNotDriveThePwm)
{
    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

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
    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });
}

TEST_F(TestRunner, PhaseCurrentsAreDispatchedToTheControlLaw)
{
    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(focMock, Enable());
    EXPECT_CALL(inverterMock, Start());
    runner.Enable();

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(_, _)).WillOnce(Return(foc::PhasePwmDutyCycles{ hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50) }));
    EXPECT_CALL(inverterMock, ThreePhasePwmOutput(_));

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });

    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());
}

TEST_F(TestRunner, DisableReleasesThePhaseCurrentsSlot)
{
    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(focMock, Enable());
    EXPECT_CALL(inverterMock, Start());
    runner.Enable();

    EXPECT_CALL(inverterMock, Stop()).Times(2);
    EXPECT_CALL(focMock, Disable()).Times(2);
    runner.Disable();

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });
}

TEST_F(TestRunner, RegisteredObserverSeesThePhaseCurrentsAfterTheDutiesAreWritten)
{
    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(focMock, Enable());
    EXPECT_CALL(inverterMock, Start());
    runner.Enable();

    bool dutiesWritten = false;
    std::optional<foc::PhaseCurrents> observed;

    runner.RegisterPhaseCurrentsObserver([&](const foc::PhaseCurrents& currents)
        {
            EXPECT_TRUE(dutiesWritten);
            observed = currents;
        });

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(_, _)).WillOnce(Return(foc::PhasePwmDutyCycles{ hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50) }));
    EXPECT_CALL(inverterMock, ThreePhasePwmOutput(_)).WillOnce([&](const foc::PhasePwmDutyCycles&)
        {
            dutiesWritten = true;
        });

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });

    ASSERT_TRUE(observed.has_value());
    EXPECT_FLOAT_EQ(1.0f, observed->a.Value());

    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());
}

TEST_F(TestRunner, UnregisteredObserverIsNotCalled)
{
    foc::Runner<foc::FocTorqueMock> runner{ inverterMock, encoderMock, focMock };

    EXPECT_CALL(focMock, Enable());
    EXPECT_CALL(inverterMock, Start());
    runner.Enable();

    bool observed = false;
    runner.RegisterPhaseCurrentsObserver([&](const foc::PhaseCurrents&)
        {
            observed = true;
        });
    runner.UnregisterPhaseCurrentsObserver();

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(_, _)).WillOnce(Return(foc::PhasePwmDutyCycles{ hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50) }));
    EXPECT_CALL(inverterMock, ThreePhasePwmOutput(_));

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });

    EXPECT_FALSE(observed);

    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(focMock, Disable());
}
