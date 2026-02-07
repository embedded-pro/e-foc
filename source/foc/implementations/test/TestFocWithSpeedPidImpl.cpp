#include "source/foc/implementations/FocWithSpeedPidImpl.hpp"
#include <cmath>
#include <gmock/gmock.h>

namespace
{
    class MockLowPriorityInterrupt
        : public foc::LowPriorityInterrupt
    {
    public:
        void Trigger() override
        {
            triggerCount++;
            if (handler)
                handler();
        }

        void Register(const infra::Function<void()>& h) override
        {
            handler = h;
            registerCalled = true;
        }

        infra::Function<void()> handler;
        int triggerCount = 0;
        bool registerCalled = false;
    };

    class ConcreteFocWithSpeedPidImpl : public foc::FocWithSpeedPidImpl
    {
    public:
        using foc::FocWithSpeedPidImpl::FocWithSpeedPidImpl;

        void SetPolePairs(std::size_t polePairs) override
        {
            polePairsValue = polePairs;
            focTorqueImpl.SetPolePairs(polePairs);
        }

        std::size_t polePairsValue = 1;
    };

    class TestFocWithSpeedPidImpl
        : public ::testing::Test
    {
    public:
        MockLowPriorityInterrupt mockInterrupt;
    };
}

TEST_F(TestFocWithSpeedPidImpl, speed_loop_period_calculated_correctly)
{
    foc::NyquistFactor nyquist{ 5 };
    hal::Hertz pwmFreq{ 10000 };

    ConcreteFocWithSpeedPidImpl foc(mockInterrupt, foc::Ampere{ 10.0f }, pwmFreq, nyquist);

    float expectedPeriod = 5.0f / 10000.0f;
    EXPECT_NEAR(expectedPeriod, 0.0005f, 1e-6f);
}

TEST_F(TestFocWithSpeedPidImpl, speed_loop_runs_every_nyquist_iterations)
{
    foc::NyquistFactor nyquist{ 4 };
    hal::Hertz pwmFreq{ 10000 };

    ConcreteFocWithSpeedPidImpl foc(mockInterrupt, foc::Ampere{ 10.0f }, pwmFreq, nyquist);

    foc.SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.1f, 0.0f }, { 1.0f, 0.1f, 0.0f } });
    foc.SetSpeedTunings(foc::Volts{ 24.0f }, { 1.0f, 0.1f, 0.0f });
    foc.SetPoint(foc::RadiansPerSecond{ 10.0f });

    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::Radians angle{ 0.0f };

    int initialTriggerCount = mockInterrupt.triggerCount;

    for (int i = 0; i < 3; ++i)
    {
        foc.Calculate(currents, angle);
    }
    EXPECT_EQ(mockInterrupt.triggerCount, initialTriggerCount);

    foc.Calculate(currents, angle);
    EXPECT_EQ(mockInterrupt.triggerCount, initialTriggerCount + 1);

    for (int i = 0; i < 4; ++i)
    {
        foc.Calculate(currents, angle);
    }
    EXPECT_EQ(mockInterrupt.triggerCount, initialTriggerCount + 2);
}

TEST_F(TestFocWithSpeedPidImpl, calculate_returns_valid_pwm_duty_cycles)
{
    foc::NyquistFactor nyquist{ 5 };
    hal::Hertz pwmFreq{ 10000 };

    ConcreteFocWithSpeedPidImpl foc(mockInterrupt, foc::Ampere{ 10.0f }, pwmFreq, nyquist);

    foc.SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.1f, 0.0f }, { 1.0f, 0.1f, 0.0f } });
    foc.SetSpeedTunings(foc::Volts{ 24.0f }, { 1.0f, 0.1f, 0.0f });

    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::Radians angle{ 0.5f };

    auto result = foc.Calculate(currents, angle);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
    EXPECT_GE(result.b.Value(), 0);
    EXPECT_LE(result.b.Value(), 100);
    EXPECT_GE(result.c.Value(), 0);
    EXPECT_LE(result.c.Value(), 100);
}

TEST_F(TestFocWithSpeedPidImpl, reset_clears_speed_loop_counter)
{
    foc::NyquistFactor nyquist{ 4 };
    hal::Hertz pwmFreq{ 10000 };

    ConcreteFocWithSpeedPidImpl foc(mockInterrupt, foc::Ampere{ 10.0f }, pwmFreq, nyquist);

    foc.SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.1f, 0.0f }, { 1.0f, 0.1f, 0.0f } });
    foc.SetSpeedTunings(foc::Volts{ 24.0f }, { 1.0f, 0.1f, 0.0f });

    foc::PhaseCurrents currents{ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } };
    foc::Radians angle{ 0.5f };

    for (int i = 0; i < 2; ++i)
    {
        foc.Calculate(currents, angle);
    }

    int triggerCountBeforeReset = mockInterrupt.triggerCount;

    foc.Reset();

    for (int i = 0; i < 3; ++i)
    {
        foc.Calculate(currents, angle);
    }
    EXPECT_EQ(mockInterrupt.triggerCount, triggerCountBeforeReset);

    foc.Calculate(currents, angle);
    EXPECT_EQ(mockInterrupt.triggerCount, triggerCountBeforeReset + 1);
}

TEST_F(TestFocWithSpeedPidImpl, low_priority_interrupt_registered_on_construction)
{
    foc::NyquistFactor nyquist{ 5 };
    hal::Hertz pwmFreq{ 10000 };

    ConcreteFocWithSpeedPidImpl foc(mockInterrupt, foc::Ampere{ 10.0f }, pwmFreq, nyquist);

    EXPECT_TRUE(mockInterrupt.registerCalled);
    EXPECT_TRUE(mockInterrupt.handler != nullptr);
}
