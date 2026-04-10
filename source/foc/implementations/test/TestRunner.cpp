#include "source/foc/implementations/Runner.hpp"
#include "source/foc/implementations/test_doubles/DriversMock.hpp"
#include "source/foc/implementations/test_doubles/FocMock.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    using testing::_;
    using testing::Invoke;
    using testing::Return;
    using testing::StrictMock;

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
            EXPECT_CALL(inverterMock, Stop()).Times(testing::AnyNumber());
            EXPECT_CALL(focMock, Disable()).Times(testing::AnyNumber());
        }

        testing::StrictMock<foc::FieldOrientedControllerInterfaceMock> inverterMock;
        testing::StrictMock<foc::EncoderMock> encoderMock;
        testing::StrictMock<foc::FocTorqueMock> focMock;
    };

    TEST_F(TestRunner, ConstructionRegistersPhaseCurrentsCallback)
    {
        EXPECT_CALL(inverterMock, PhaseCurrentsReady(hal::Hertz{ 20000 }, _));

        foc::Runner runner{ inverterMock, encoderMock, focMock };
    }

    TEST_F(TestRunner, EnableStartsFocThenInverter)
    {
        foc::Runner runner{ inverterMock, encoderMock, focMock };

        testing::InSequence seq;
        EXPECT_CALL(focMock, Enable());
        EXPECT_CALL(inverterMock, Start());

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
            .WillOnce([this](hal::Hertz, const infra::Function<void(foc::PhaseCurrents)>& onDone)
                {
                    inverterMock.StorePhaseCurrentsCallback(onDone);
                });

        foc::Runner runner{ inverterMock, encoderMock, focMock };

        const foc::PhaseCurrents testCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
        const foc::PhasePwmDutyCycles expectedDuties{ hal::Percent{ 60 }, hal::Percent{ 30 }, hal::Percent{ 10 } };

        {
            testing::InSequence seq;
            EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.5f }));
            EXPECT_CALL(focMock, Calculate(_, _)).WillOnce(Return(expectedDuties));
            EXPECT_CALL(inverterMock, ThreePhasePwmOutput(_));
        }

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
}
