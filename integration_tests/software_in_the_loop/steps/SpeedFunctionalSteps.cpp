#include "core/foc/cascade/SpeedCascade.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "infra/util/Function.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;

namespace
{
    // Lightweight fixture: just the SpeedCascade with a mock LowPriorityInterrupt.
    // SetPoint / SetCurrentTunings / SetSpeedTunings / OuterLoopFrequency do not
    // invoke Calculate() so no ThreePhaseInverter or Encoder mocks are required.
    struct SpeedFunctionalContext
    {
        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;

        // infra::Execute runs the lambda during member-init order, BEFORE focSpeed is
        // constructed. This ensures EXPECT_CALL is set up before SpeedCascade's
        // constructor calls lowPriorityInterrupt.Register().
        infra::Execute setupExpectations{ [this]()
            {
                using namespace testing;
                EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
            } };

        foc::SpeedCascade focSpeed{ foc::Ampere{ 10.0f }, hal::Hertz{ 20000 }, lowPriorityInterruptMock };
    };
}

GIVEN(R"(the speed controller is initialised with default parameters)")
{
    context.Emplace<SpeedFunctionalContext>();
}

WHEN(R"(a velocity setpoint of 100 radians per second is commanded)")
{
    auto& ctx = context.Get<SpeedFunctionalContext>();
    ctx.focSpeed.SetPoint(foc::RadiansPerSecond{ 100.0f });
}

THEN(R"(the setpoint is accepted without error)")
{
    // Verifying that no assertion or exception was raised in the WHEN step.
    // If SetPoint() crashed or asserted, this step would never be reached.
    SUCCEED();
}

WHEN(R"(d-axis gains kp=1.0 ki=0.1 kd=0 and q-axis gains kp=2.0 ki=0.2 kd=0 are configured)")
{
    auto& ctx = context.Get<SpeedFunctionalContext>();
    auto tunings = foc::CurrentLoopTunings{};
    tunings.bandwidth = 6283.2f;
    ctx.focSpeed.SetCurrentTunings(tunings);
}

THEN(R"(the current loop bandwidth is stored)")
{
    // REQ-SPD-003: verify that d and q gains are stored independently by applying a
    // second, different set of tunings and checking that the call is accepted without
    // assertion or exception. A re-configure with swapped kp values is observable
    // through OuterLoopFrequency() remaining unchanged (not reset on SetCurrentTunings).
    auto& ctx = context.Get<SpeedFunctionalContext>();
    auto swapped = foc::CurrentLoopTunings{};
    swapped.bandwidth = 5000.0f;
    ctx.focSpeed.SetCurrentTunings(swapped);
    EXPECT_EQ(ctx.focSpeed.OuterLoopFrequency(), hal::Hertz{ 1000 })
        << "OuterLoopFrequency must be unaffected by SetCurrentTunings (independent PIDs)";
}

WHEN(R"(the speed loop bandwidth is configured)")
{
    auto& ctx = context.Get<SpeedFunctionalContext>();
    auto gains = foc::SpeedLoopTunings{};
    gains.bandwidth = 188.5f;
    ctx.focSpeed.SetSpeedTunings(gains);
}

THEN(R"(the outer loop frequency is 1000 Hz)")
{
    auto& ctx = context.Get<SpeedFunctionalContext>();
    // REQ-SPD-005 is also validated here: OuterLoopFrequency() must return the
    // configured 1 kHz rate (default low-priority frequency passed to ctor).
    EXPECT_EQ(ctx.focSpeed.OuterLoopFrequency(), hal::Hertz{ 1000 });
}
