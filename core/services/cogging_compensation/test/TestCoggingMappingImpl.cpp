#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/foc/interfaces/test_doubles/FocMock.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/cogging_compensation/CoggingMappingImpl.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/WithSharedAccess.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    using namespace testing;

    constexpr uint32_t baseFrequency{ 20000 };
    constexpr float samplePeriod{ 1.0f / static_cast<float>(baseFrequency) };
    constexpr uint32_t ticksPerMillisecond{ baseFrequency / 1000 };
    constexpr float twoPi{ 2.0f * std::numbers::pi_v<float> };
    constexpr float torqueConstant{ 0.042f };
    constexpr std::size_t polePairs{ 4 };
    constexpr float coggingTorque{ 4.0e-3f };
    constexpr float dryFriction{ 3.7e-3f };

    foc::PhaseCurrents CurrentsProducing(float torque, float position)
    {
        const auto electricalAngle = position * static_cast<float>(polePairs);
        const auto cosine = foc::FastTrigonometry::Cosine(electricalAngle);
        const auto sine = foc::FastTrigonometry::Sine(electricalAngle);
        const foc::ClarkePark transform;

        const auto unitGain = transform.Forward(transform.Inverse(foc::RotatingFrame{ 0.0f, 1.0f }, cosine, sine), cosine, sine).q;
        const auto phases = transform.Inverse(foc::RotatingFrame{ 0.0f, torque / torqueConstant / unitGain }, cosine, sine);

        return foc::PhaseCurrents{ foc::Ampere{ phases.a }, foc::Ampere{ phases.b }, foc::Ampere{ phases.c } };
    }

    class TestCoggingMappingImpl
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        void TearDown() override
        {
            ExecuteAllActions();
        }

        void ExpectRunStarted()
        {
            EXPECT_CALL(observableMock, RegisterPhaseCurrentsObserver(_))
                .WillOnce(Invoke([this](const infra::Function<void(const foc::PhaseCurrents&)>& observer)
                    {
                        observableMock.StoreObserver(observer);
                    }));
            EXPECT_CALL(driveMock, Start());
            EXPECT_CALL(controllerMock, EnableSpeedCommand());
            EXPECT_CALL(controllerMock, CommandSpeed(_)).WillRepeatedly(Invoke([this](foc::RadiansPerSecond speed)
                {
                    commandedSpeed = speed.Value();
                }));
            EXPECT_CALL(encoderMock, Read()).WillRepeatedly(Invoke([this]()
                {
                    return foc::Radians{ std::fmod(position, twoPi) };
                }));
        }

        void ExpectDriveReleased()
        {
            EXPECT_CALL(driveMock, Stop());
            EXPECT_CALL(controllerMock, DisableSpeedCommand());
            EXPECT_CALL(observableMock, UnregisterPhaseCurrentsObserver()).WillOnce(Invoke([this]()
                {
                    observableMock.ReleaseObserver();
                }));
        }

        void Start(services::CoggingMapping::Config config = {})
        {
            mapping->Measure(foc::NewtonMeter{ torqueConstant }, foc::NewtonMeterSecondSquared{ 14.0e-6f }, polePairs, config, [this](std::optional<services::CoggingMap> map)
                {
                    result = map;
                    reported = true;
                });
        }

        void Run(float rotorSpeedScale = 1.0f, uint32_t maxMilliseconds = 20000)
        {
            for (uint32_t millisecond = 0; millisecond != maxMilliseconds && !reported; ++millisecond)
            {
                for (uint32_t tick = 0; tick != ticksPerMillisecond; ++tick)
                {
                    const auto speed = rotorSpeedScale * commandedSpeed;
                    position += speed * samplePeriod;
                    const auto friction = speed > 0.0f ? dryFriction : (speed < 0.0f ? -dryFriction : 0.0f);
                    observableMock.Publish(CurrentsProducing(Cogging(position) + friction, position));
                }

                ForwardTime(std::chrono::milliseconds{ 1 });
            }
        }

        static float Cogging(float mechanicalAngle)
        {
            return coggingTorque * std::sin(24.0f * mechanicalAngle);
        }

        StrictMock<foc::SpeedCommandableMock> controllerMock;
        StrictMock<foc::ControllableMock> driveMock;
        StrictMock<foc::PhaseCurrentsObservableMock> observableMock;
        StrictMock<drivers::ThreePhaseInverterMock> inverterMock;
        StrictMock<drivers::EncoderMock> encoderMock;
        infra::Execute setFrequency{ [this]()
            {
                EXPECT_CALL(inverterMock, BaseFrequency()).WillRepeatedly(Return(hal::Hertz{ baseFrequency }));
            } };
        infra::WithSharedAccess<services::CoggingMappingImpl> mapping{ controllerMock, driveMock, observableMock, inverterMock, encoderMock };

        float position{ 0.0f };
        float commandedSpeed{ 0.0f };
        std::optional<services::CoggingMap> result;
        bool reported{ false };
    };
}

TEST_F(TestCoggingMappingImpl, turning_each_way_maps_the_cogging_the_winding_held_against)
{
    ExpectRunStarted();
    Start();
    EXPECT_TRUE(mapping->IsRunning());

    ExpectDriveReleased();
    Run();

    ASSERT_TRUE(reported);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(mapping->IsRunning());
    EXPECT_NEAR(result->peak.Value(), coggingTorque, 0.05f * coggingTorque);

    const auto binWidth = twoPi / static_cast<float>(services::coggingMapBins);
    for (std::size_t bin = 0; bin != services::coggingMapBins; ++bin)
    {
        const auto centre = (static_cast<float>(bin) + 0.5f) * binWidth / static_cast<float>(polePairs);
        EXPECT_NEAR(static_cast<float>(result->torque[bin]) * result->torqueScale, 0.9745f * Cogging(centre), 0.1f * coggingTorque) << "bin " << bin;
    }
}

TEST_F(TestCoggingMappingImpl, a_phase_current_beyond_the_envelope_stops_the_bridge_and_yields_no_map)
{
    ExpectRunStarted();
    Start();

    EXPECT_CALL(inverterMock, Stop());
    observableMock.Publish(foc::PhaseCurrents{ foc::Ampere{ 50.0f }, foc::Ampere{ -25.0f }, foc::Ampere{ -25.0f } });

    ExpectDriveReleased();
    ExecuteAllActions();

    EXPECT_TRUE(reported);
    EXPECT_FALSE(result.has_value());
}

TEST_F(TestCoggingMappingImpl, a_rotor_that_does_not_turn_runs_into_the_timeout_without_a_map)
{
    ExpectRunStarted();
    services::CoggingMapping::Config config{};
    config.timeout = std::chrono::seconds{ 2 };
    Start(config);

    ExpectDriveReleased();
    Run(0.0f, 2500);

    EXPECT_TRUE(reported);
    EXPECT_FALSE(result.has_value());
}

TEST_F(TestCoggingMappingImpl, an_aborted_run_releases_the_drive_and_never_reports)
{
    ExpectRunStarted();
    Start();
    Run(1.0f, 100);

    ExpectDriveReleased();
    mapping->Abort();
    ForwardTime(std::chrono::seconds{ 20 });

    EXPECT_FALSE(reported);
    EXPECT_FALSE(mapping->IsRunning());
}

TEST_F(TestCoggingMappingImpl, a_request_without_pole_pairs_is_refused_at_once)
{
    mapping->Measure(foc::NewtonMeter{ torqueConstant }, foc::NewtonMeterSecondSquared{ 14.0e-6f }, 0, {}, [this](std::optional<services::CoggingMap> map)
        {
            result = map;
            reported = true;
        });

    EXPECT_TRUE(reported);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(mapping->IsRunning());
}
