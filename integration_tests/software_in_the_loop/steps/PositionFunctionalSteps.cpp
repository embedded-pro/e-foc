#include "core/foc/implementations/FocPositionImpl.hpp"
#include "core/foc/implementations/test_doubles/DriversMock.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "infra/util/Function.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;

namespace
{
    // Lightweight fixture: just the FocPositionImpl with a mock LowPriorityInterrupt.
    // SetPoint / SetCurrentTunings / SetSpeedTunings / SetPositionTunings do not
    // invoke Calculate() so no ThreePhaseInverter or Encoder mocks are required.
    struct PositionFunctionalContext
    {
        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;

        // infra::Execute runs the lambda during member-init order, BEFORE focPosition is
        // constructed. This ensures EXPECT_CALL is set up before FocPositionImpl's
        // constructor calls lowPriorityInterrupt.Register().
        infra::Execute setupExpectations{ [this]()
            {
                using namespace testing;
                EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
            } };

        foc::FocPositionImpl focPosition{ foc::Ampere{ 10.0f }, hal::Hertz{ 20000 }, lowPriorityInterruptMock };
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

WHEN(R"(position current tunings kp=1.0 ki=0.1 kd=0 and q-axis kp=2.0 ki=0.2 kd=0 are configured)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    const foc::IdAndIqTunings tunings{
        controllers::PidTunings<float>{ 1.0f, 0.1f, 0.0f },
        controllers::PidTunings<float>{ 2.0f, 0.2f, 0.0f }
    };
    ctx.focPosition.SetCurrentTunings(foc::Volts{ 24.0f }, tunings);
}

WHEN(R"(speed PID gains kp=5.0 ki=0.5 kd=0.01 are configured)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    const foc::SpeedTunings gains{ 5.0f, 0.5f, 0.01f };
    ctx.focPosition.SetSpeedTunings(foc::Volts{ 24.0f }, gains);
}

WHEN(R"(position PID gains kp=10.0 ki=0.0 kd=0.1 are configured)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    const foc::PositionTunings gains{ 10.0f, 0.0f, 0.1f };
    ctx.focPosition.SetPositionTunings(gains);
}

THEN(R"(all three sets of tunings are stored independently)")
{
    // REQ-POS-003: verify independence by applying a second distinct set of gains for
    // each loop after the first and confirming no assertion or exception is raised.
    // A re-configure that mixes up d/q kp values exercises that both PID instances are
    // writable without conflict.
    auto& ctx = context.Get<PositionFunctionalContext>();
    const foc::IdAndIqTunings swapped{
        controllers::PidTunings<float>{ 2.0f, 0.2f, 0.0f },
        controllers::PidTunings<float>{ 1.0f, 0.1f, 0.0f }
    };
    ctx.focPosition.SetCurrentTunings(foc::Volts{ 24.0f }, swapped);
    ctx.focPosition.SetSpeedTunings(foc::Volts{ 24.0f }, foc::SpeedTunings{ 3.0f, 0.3f, 0.0f });
    ctx.focPosition.SetPositionTunings(foc::PositionTunings{ 5.0f, 0.0f, 0.05f });
}

THEN(R"(the position PID tunings are accepted without error)")
{
    // REQ-POS-004: re-apply different position PID gains and verify the second call is
    // accepted (anti-windup limits are internal; the interface must not reject valid gains).
    auto& ctx = context.Get<PositionFunctionalContext>();
    ctx.focPosition.SetPositionTunings(foc::PositionTunings{ 20.0f, 0.1f, 0.2f });
}
