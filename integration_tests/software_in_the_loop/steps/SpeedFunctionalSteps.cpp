#include "core/foc/cascade/SpeedCascade.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "cucumber_cpp/Steps.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>

using namespace testing;

namespace
{
    const hal::Hertz baseFrequency{ 20000 };
    const hal::Hertz outerLoopFrequency{ 1000 };

    constexpr float nominalCurrentBandwidth{ 628.3185f };
    constexpr float detuneFactor{ 100.0f };

    constexpr float speedStep{ 2.0f };

    foc::MotorModelParameters MotorParameters()
    {
        auto parameters = foc::MotorModelParameters{};
        parameters.resistance = foc::Ohm{ 1.0f };
        parameters.inductance = foc::MilliHenry{ 1.0f };
        parameters.fluxLinkage = foc::Weber{ 0.01f };
        parameters.busVoltage = foc::Volts{ 24.0f };
        parameters.samplingFrequency = baseFrequency;
        parameters.polePairs = 7;
        return parameters;
    }

    foc::MechanicalModelParameters MechanicalParameters()
    {
        auto parameters = foc::MechanicalModelParameters{};
        parameters.inertia = foc::NewtonMeterSecondSquared{ 0.001f };
        parameters.viscousFriction = foc::NewtonMeterSecondPerRadian{ 0.0001f };
        parameters.torqueConstant = foc::NewtonMeter{ 0.1f };
        parameters.maxCurrent = foc::Ampere{ 10.0f };
        parameters.samplingFrequency = outerLoopFrequency;
        return parameters;
    }

    foc::CurrentLoopTunings CurrentTunings(float bandwidth)
    {
        auto tunings = foc::CurrentLoopTunings{};
        tunings.bandwidth = bandwidth;
        return tunings;
    }

    foc::PhaseCurrents ZeroCurrents()
    {
        return { foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    }

    bool DutiesDiffer(const foc::PhasePwmDutyCycles& left, const foc::PhasePwmDutyCycles& right)
    {
        return left.a.Value() != right.a.Value() ||
               left.b.Value() != right.b.Value() ||
               left.c.Value() != right.c.Value();
    }

    struct SpeedCascadeUnderTest
    {
        explicit SpeedCascadeUnderTest(float currentBandwidth)
        {
            EXPECT_CALL(lowPriorityInterrupt, Register(_)).WillOnce(Invoke(&lowPriorityInterrupt, &foc::LowPriorityInterruptMock::StoreHandler));
            EXPECT_CALL(lowPriorityInterrupt, Unregister()).WillOnce(Invoke(&lowPriorityInterrupt, &foc::LowPriorityInterruptMock::ClearHandler));

            cascade.emplace(foc::Ampere{ 10.0f }, baseFrequency, lowPriorityInterrupt, outerLoopFrequency);
            cascade->Configure(MotorParameters());
            cascade->ConfigureMechanics(MechanicalParameters());
            cascade->SetCurrentTunings(CurrentTunings(currentBandwidth));
            cascade->SetSpeedTunings(foc::SpeedLoopTunings{});
        }

        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterrupt;
        std::optional<foc::SpeedCascade> cascade;
    };

    foc::PhasePwmDutyCycles DutyAfterOuterCycle(SpeedCascadeUnderTest& speedCascade)
    {
        speedCascade.cascade->Enable();

        foc::Radians position{ 0.0f };
        speedCascade.cascade->Calculate(ZeroCurrents(), position);
        speedCascade.lowPriorityInterrupt.TriggerHandler();

        foc::Radians secondSample{ 0.0f };
        return speedCascade.cascade->Calculate(ZeroCurrents(), secondSample);
    }

    struct SpeedFunctionalContext
    {
        SpeedCascadeUnderTest underTest{ nominalCurrentBandwidth };
    };
}

GIVEN(R"(the speed controller is initialised with default parameters)")
{
    context.Emplace<SpeedFunctionalContext>();
}

WHEN(R"(a velocity setpoint of 100 radians per second is commanded)")
{
    auto& ctx = context.Get<SpeedFunctionalContext>();
    ctx.underTest.cascade->SetPoint(foc::RadiansPerSecond{ 100.0f });
}

THEN(R"(the commanded duty cycles follow the velocity setpoint)")
{
    auto& ctx = context.Get<SpeedFunctionalContext>();
    auto commanded = DutyAfterOuterCycle(ctx.underTest);

    SpeedCascadeUnderTest atStandstill{ nominalCurrentBandwidth };
    atStandstill.cascade->SetPoint(foc::RadiansPerSecond{ 0.0f });
    auto held = DutyAfterOuterCycle(atStandstill);

    EXPECT_TRUE(DutiesDiffer(commanded, held))
        << "a commanded velocity must drive the modulator away from the standstill command";
}

WHEN(R"(a current loop bandwidth well below the baseline is configured)")
{
    auto& ctx = context.Get<SpeedFunctionalContext>();
    ctx.underTest.cascade->SetCurrentTunings(CurrentTunings(nominalCurrentBandwidth / detuneFactor));
}

THEN(R"(the commanded duty cycles differ from those of the baseline bandwidth)")
{
    auto& ctx = context.Get<SpeedFunctionalContext>();
    ctx.underTest.cascade->SetPoint(foc::RadiansPerSecond{ speedStep });
    auto narrowBandwidth = DutyAfterOuterCycle(ctx.underTest);

    SpeedCascadeUnderTest wideBandwidth{ nominalCurrentBandwidth };
    wideBandwidth.cascade->SetPoint(foc::RadiansPerSecond{ speedStep });
    auto wide = DutyAfterOuterCycle(wideBandwidth);

    EXPECT_TRUE(DutiesDiffer(narrowBandwidth, wide))
        << "the current loop must act on the configured bandwidth for the same current error";
}

WHEN(R"(the speed loop bandwidth is configured)")
{
    auto& ctx = context.Get<SpeedFunctionalContext>();
    auto gains = foc::SpeedLoopTunings{};
    gains.bandwidth = 188.5f;
    ctx.underTest.cascade->SetSpeedTunings(gains);
}

THEN(R"(the outer loop frequency is 1000 Hz)")
{
    auto& ctx = context.Get<SpeedFunctionalContext>();
    EXPECT_EQ(ctx.underTest.cascade->OuterLoopFrequency(), outerLoopFrequency);
}
