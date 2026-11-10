#include "core/foc/instantiations/FocController.hpp"
#include "core/foc/interfaces/test_doubles/FocMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    using testing::_;
    using testing::Return;

    class TestFocController
        : public testing::Test
    {
    public:
        TestFocController()
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
    };
}

TEST_F(TestFocController, StartEnablesTheFocAlgorithmThenTheInverter)
{
    foc::FocController<testing::StrictMock<foc::FocTorqueMock>> controller{ inverterMock, encoderMock };

    testing::InSequence seq;
    EXPECT_CALL(controller, Enable());
    EXPECT_CALL(inverterMock, Start());
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(controller, Disable());

    controller.Start();
}

TEST_F(TestFocController, StopDisablesTheInverterThenTheFocAlgorithm)
{
    foc::FocController<testing::StrictMock<foc::FocTorqueMock>> controller{ inverterMock, encoderMock };

    testing::InSequence seq;
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(controller, Disable());
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(controller, Disable());

    controller.Stop();
}

TEST_F(TestFocController, RegisteredObserverIsInvokedForEveryPhaseCurrentsSample)
{
    foc::FocController<testing::StrictMock<foc::FocTorqueMock>> controller{ inverterMock, encoderMock };

    EXPECT_CALL(controller, Enable());
    EXPECT_CALL(inverterMock, Start());
    controller.Start();

    bool observed = false;
    controller.RegisterPhaseCurrentsObserver([&](const foc::PhaseCurrents&)
        {
            observed = true;
        });

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(controller, Calculate(_, _)).WillOnce(Return(foc::PhasePwmDutyCycles{ hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50) }));
    EXPECT_CALL(inverterMock, ThreePhasePwmOutput(_));

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });

    EXPECT_TRUE(observed);

    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(controller, Disable());
}

TEST_F(TestFocController, UnregisteredObserverIsNotInvoked)
{
    foc::FocController<testing::StrictMock<foc::FocTorqueMock>> controller{ inverterMock, encoderMock };

    EXPECT_CALL(controller, Enable());
    EXPECT_CALL(inverterMock, Start());
    controller.Start();

    bool observed = false;
    controller.RegisterPhaseCurrentsObserver([&](const foc::PhaseCurrents&)
        {
            observed = true;
        });
    controller.UnregisterPhaseCurrentsObserver();

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(controller, Calculate(_, _)).WillOnce(Return(foc::PhasePwmDutyCycles{ hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50) }));
    EXPECT_CALL(inverterMock, ThreePhasePwmOutput(_));

    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });

    EXPECT_FALSE(observed);

    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(controller, Disable());
}
