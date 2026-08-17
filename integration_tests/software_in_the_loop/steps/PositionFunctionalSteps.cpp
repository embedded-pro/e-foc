#include "core/foc/cascade/PositionCascade.hpp"
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

    // The shipped current-loop bandwidth rails the modulator for every reference an outer loop can
    // ask for, which would hide the differences these scenarios look for.
    constexpr float nominalCurrentBandwidth{ 628.3185f };
    const float nominalSpeedBandwidth{ foc::SpeedLoopTunings{}.bandwidth };
    const float nominalPositionBandwidth{ foc::PositionLoopTunings{}.bandwidth };
    constexpr float detuneFactor{ 100.0f };

    // Small enough to keep the modulator off its rails, large enough to move it by more than the
    // one percent quantisation of hal::Percent
    constexpr float positionStep{ 0.05f };

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

    foc::SpeedLoopTunings SpeedTunings(float bandwidth)
    {
        auto tunings = foc::SpeedLoopTunings{};
        tunings.bandwidth = bandwidth;
        return tunings;
    }

    foc::PositionLoopTunings PositionTunings(float bandwidth)
    {
        auto tunings = foc::PositionLoopTunings{};
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

    void ExpectSameDuty(const foc::PhasePwmDutyCycles& duty, const foc::PhasePwmDutyCycles& expected)
    {
        EXPECT_EQ(duty.a.Value(), expected.a.Value());
        EXPECT_EQ(duty.b.Value(), expected.b.Value());
        EXPECT_EQ(duty.c.Value(), expected.c.Value());
    }

    struct PositionCascadeUnderTest
    {
        PositionCascadeUnderTest(float currentBandwidth, float speedBandwidth, float positionBandwidth)
        {
            EXPECT_CALL(lowPriorityInterrupt, Register(_)).WillOnce(Invoke(&lowPriorityInterrupt, &foc::LowPriorityInterruptMock::StoreHandler));
            EXPECT_CALL(lowPriorityInterrupt, Unregister()).WillOnce(Invoke(&lowPriorityInterrupt, &foc::LowPriorityInterruptMock::ClearHandler));

            cascade.emplace(foc::Ampere{ 10.0f }, baseFrequency, lowPriorityInterrupt, outerLoopFrequency);
            cascade->Configure(MotorParameters());
            cascade->ConfigureMechanics(MechanicalParameters());
            cascade->SetCurrentTunings(CurrentTunings(currentBandwidth));
            cascade->SetSpeedTunings(SpeedTunings(speedBandwidth));
            EXPECT_EQ(cascade->SetPositionTunings(PositionTunings(positionBandwidth)), foc::SelectResult::ok);
        }

        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterrupt;
        std::optional<foc::PositionCascade> cascade;
    };

    // Enable re-applies the standing setpoint, so a setpoint commanded in a WHEN step survives;
    // the cascade is left disabled because retuning is rejected while it runs.
    foc::PhasePwmDutyCycles DutyAfterOuterCycle(PositionCascadeUnderTest& positionCascade)
    {
        positionCascade.cascade->Enable();

        foc::Radians position{ 0.0f };
        positionCascade.cascade->Calculate(ZeroCurrents(), position);
        positionCascade.lowPriorityInterrupt.TriggerHandler();

        foc::Radians secondSample{ 0.0f };
        auto duty = positionCascade.cascade->Calculate(ZeroCurrents(), secondSample);

        positionCascade.cascade->Disable();
        return duty;
    }

    foc::PhasePwmDutyCycles DutyAtPositionError(PositionCascadeUnderTest& positionCascade, float error)
    {
        positionCascade.cascade->SetPoint(foc::Radians{ error });
        return DutyAfterOuterCycle(positionCascade);
    }

    struct PositionFunctionalContext
    {
        // The position loop starts detuned so the scenario that configures it changes something
        PositionCascadeUnderTest underTest{ nominalCurrentBandwidth, nominalSpeedBandwidth, nominalPositionBandwidth / detuneFactor };
    };
}

GIVEN(R"(the position controller is initialised with default parameters)")
{
    context.Emplace<PositionFunctionalContext>();
}

WHEN(R"(a position setpoint of 3.14 radians is commanded)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    ctx.underTest.cascade->SetPoint(foc::Radians{ 3.14f });
}

THEN(R"(the commanded duty cycles follow the position setpoint)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    auto commanded = DutyAfterOuterCycle(ctx.underTest);

    PositionCascadeUnderTest atTarget{ nominalCurrentBandwidth, nominalSpeedBandwidth, nominalPositionBandwidth / detuneFactor };
    auto held = DutyAtPositionError(atTarget, 0.0f);

    EXPECT_TRUE(DutiesDiffer(commanded, held))
        << "a commanded position must drive the modulator away from the on-target command";
}

WHEN(R"(the position current loop bandwidth is configured)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    ctx.underTest.cascade->SetCurrentTunings(CurrentTunings(nominalCurrentBandwidth));
}

WHEN(R"(the cascade speed loop bandwidth is configured)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    ctx.underTest.cascade->SetSpeedTunings(SpeedTunings(nominalSpeedBandwidth));
}

WHEN(R"(the position loop bandwidth is configured)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    EXPECT_EQ(ctx.underTest.cascade->SetPositionTunings(PositionTunings(nominalPositionBandwidth)), foc::SelectResult::ok);
}

THEN(R"(each configured bandwidth acts on its own loop)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    auto configured = DutyAtPositionError(ctx.underTest, positionStep);

    PositionCascadeUnderTest sameTunings{ nominalCurrentBandwidth, nominalSpeedBandwidth, nominalPositionBandwidth };
    PositionCascadeUnderTest currentDetuned{ nominalCurrentBandwidth / detuneFactor, nominalSpeedBandwidth, nominalPositionBandwidth };
    PositionCascadeUnderTest speedDetuned{ nominalCurrentBandwidth, nominalSpeedBandwidth / detuneFactor, nominalPositionBandwidth };
    PositionCascadeUnderTest positionDetuned{ nominalCurrentBandwidth, nominalSpeedBandwidth, nominalPositionBandwidth / detuneFactor };

    ExpectSameDuty(configured, DutyAtPositionError(sameTunings, positionStep));
    EXPECT_TRUE(DutiesDiffer(configured, DutyAtPositionError(currentDetuned, positionStep)));
    EXPECT_TRUE(DutiesDiffer(configured, DutyAtPositionError(speedDetuned, positionStep)));
    EXPECT_TRUE(DutiesDiffer(configured, DutyAtPositionError(positionDetuned, positionStep)));
}

THEN(R"(the commanded duty cycles differ from those of the detuned position loop)")
{
    auto& ctx = context.Get<PositionFunctionalContext>();
    auto configured = DutyAtPositionError(ctx.underTest, positionStep);

    PositionCascadeUnderTest detuned{ nominalCurrentBandwidth, nominalSpeedBandwidth, nominalPositionBandwidth / detuneFactor };

    EXPECT_TRUE(DutiesDiffer(configured, DutyAtPositionError(detuned, positionStep)))
        << "the position loop must act on the configured bandwidth for the same position error";
}
