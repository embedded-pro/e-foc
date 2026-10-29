#include "core/platform_abstraction/DecimatingThreePhaseInverter.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include <gmock/gmock.h>
#include <vector>

namespace
{
    using namespace testing;

    constexpr uint32_t pwmFrequency = 20000;

    class TestDecimatingThreePhaseInverter
        : public ::testing::Test
    {
    public:
        TestDecimatingThreePhaseInverter()
        {
            EXPECT_CALL(powerStage, BaseFrequency()).WillRepeatedly(Return(hal::Hertz{ pwmFrequency }));
        }

        void Request(uint32_t frequency)
        {
            EXPECT_CALL(powerStage, PhaseCurrentsReady(_)).WillOnce(SaveArg<0>(&sample));
            inverter.PhaseCurrentsReady(hal::Hertz{ frequency }, [this](foc::PhaseCurrents currents)
                {
                    delivered.push_back(currents.a.Value());
                });
        }

        void Sample(int count)
        {
            for (int i = 1; i <= count; ++i)
                sample(foc::PhaseCurrents{ foc::Ampere{ static_cast<float>(i) }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
        }

        StrictMock<drivers::ThreePhasePowerStageMock> powerStage;
        application::DecimatingThreePhaseInverter inverter{ powerStage };
        infra::Function<void(foc::PhaseCurrents)> sample;
        std::vector<float> delivered;
    };
}

TEST_F(TestDecimatingThreePhaseInverter, at_the_pwm_frequency_every_sample_is_delivered)
{
    Request(pwmFrequency);
    Sample(3);

    EXPECT_THAT(delivered, ElementsAre(1.0f, 2.0f, 3.0f));
}

TEST_F(TestDecimatingThreePhaseInverter, a_slower_frequency_delivers_every_nth_sample)
{
    Request(pwmFrequency / 4);
    Sample(12);

    EXPECT_THAT(delivered, ElementsAre(4.0f, 8.0f, 12.0f));
}

TEST_F(TestDecimatingThreePhaseInverter, a_new_request_restarts_the_count)
{
    Request(pwmFrequency / 4);
    Sample(3);

    Request(pwmFrequency / 2);
    Sample(4);

    EXPECT_THAT(delivered, ElementsAre(2.0f, 4.0f));
}

TEST_F(TestDecimatingThreePhaseInverter, returning_to_the_pwm_frequency_delivers_every_sample_again)
{
    Request(pwmFrequency / 4);
    Sample(4);

    Request(pwmFrequency);
    Sample(2);

    EXPECT_THAT(delivered, ElementsAre(4.0f, 1.0f, 2.0f));
}

TEST_F(TestDecimatingThreePhaseInverter, forwards_the_power_stage_controls)
{
    EXPECT_CALL(powerStage, ThreePhasePwmOutput(_));
    EXPECT_CALL(powerStage, Start());
    EXPECT_CALL(powerStage, Stop());
    EXPECT_CALL(powerStage, MaxCurrentSupported()).WillOnce(Return(foc::Ampere{ 2.5f }));

    inverter.ThreePhasePwmOutput(foc::PhasePwmDutyCycles{});
    inverter.Start();
    inverter.Stop();

    EXPECT_NEAR(inverter.MaxCurrentSupported().Value(), 2.5f, 1e-6f);
    EXPECT_EQ(inverter.BaseFrequency(), hal::Hertz{ pwmFrequency });
}
