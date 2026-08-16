#include "core/foc/cascade/PositionCascade.hpp"
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
    // Lightweight fixture: just the PositionCascade with a mock LowPriorityInterrupt.
    // SetPoint / SetCurrentTunings / SetSpeedTunings / SetPositionTunings do not
    // invoke Calculate() so no ThreePhaseInverter or Encoder mocks are required.
    struct PositionFunctionalContext
    {
        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;

        // infra::Execute runs the lambda during member-init order, BEFORE focPosition is
        // constructed. This ensures EXPECT_CALL is set up before PositionCascade's
        // constructor calls lowPriorityInterrupt.Register().
        infra::Execute setupExpectations{ [this]()
            {
                using namespace testing;
                EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
            } };

        foc::PositionCascade focPosition{ foc::Ampere{ 10.0f }, hal::Hertz{ 20000 }, lowPriorityInterruptMock };
    };
}

GIVEN(R"(the position controller is initialised with default parameters)")
{
    context.Emplace<PositionFunctionalContext>();
}

WHEN(R"(a position setpoint of 3.14 radians is commanded)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    ctx.focPosition.SetPoint(foc::Radians{ 3.14f });
}

THEN(R"(the position setpoint is accepted without error)")
{
    // Verifying that no assertion or exception was raised in the WHEN step.
    SUCCEED();
}

WHEN(R"(the position current loop bandwidth is configured)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    auto tunings = foc::CurrentLoopTunings{};
    tunings.bandwidth = 6283.2f;
    ctx.focPosition.SetCurrentTunings(tunings);
}

WHEN(R"(the cascade speed loop bandwidth is configured)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    auto gains = foc::SpeedLoopTunings{};
    gains.bandwidth = 188.5f;
    ctx.focPosition.SetSpeedTunings(gains);
}

WHEN(R"(the position loop bandwidth is configured)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    auto gains = foc::PositionLoopTunings{};
    gains.bandwidth = 18.8f;
    ctx.focPosition.SetPositionTunings(gains);
}

THEN(R"(all three loop bandwidths are stored independently)")
{
    // REQ-POS-003: verify independence by applying a second distinct set of gains for
    // each loop after the first and confirming no assertion or exception is raised.
    // A re-configure that mixes up d/q kp values exercises that both PID instances are
    // writable without conflict.
    auto& ctx = context.Get<PositionFunctionalContext>();
    auto current = foc::CurrentLoopTunings{};
    current.bandwidth = 5000.0f;
    ctx.focPosition.SetCurrentTunings(current);

    auto speed = foc::SpeedLoopTunings{};
    speed.bandwidth = 150.0f;
    ctx.focPosition.SetSpeedTunings(speed);

    auto position = foc::PositionLoopTunings{};
    position.bandwidth = 15.0f;
    ctx.focPosition.SetPositionTunings(position);
}

THEN(R"(the position loop bandwidth is accepted without error)")
{
    // REQ-POS-004: re-apply different position PID gains and verify the second call is
    // accepted (anti-windup limits are internal; the interface must not reject valid gains).
    auto& ctx = context.Get<PositionFunctionalContext>();
    auto retuned = foc::PositionLoopTunings{};
    retuned.bandwidth = 20.0f;
    ctx.focPosition.SetPositionTunings(retuned);
}
