#include "source/foc/implementations/TorqueControllerImpl.hpp"
#include "source/foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "source/foc/implementations/test_doubles/DriversMock.hpp"
#include "source/foc/implementations/test_doubles/FieldOrientedControllerMock.hpp"
#include "source/foc/interfaces/FieldOrientedController.hpp"
#include "gmock/gmock.h"
#include <numbers>

namespace
{
    using namespace testing;

    MATCHER_P2(IdAndIqTuningsNear, expectedKp, expectedKi, "")
    {
        constexpr float tolerance = 1e-4f;
        return std::abs(arg.first.kp - expectedKp) < tolerance &&
               std::abs(arg.first.ki - expectedKi) < tolerance &&
               std::abs(arg.first.kd - 0.0f) < tolerance &&
               std::abs(arg.second.kp - expectedKp) < tolerance &&
               std::abs(arg.second.ki - expectedKi) < tolerance &&
               std::abs(arg.second.kd - 0.0f) < tolerance;
    }

    class TestTorqueControllerWithAutomaticPidGains
        : public ::testing::Test
    {
    public:
        TestTorqueControllerWithAutomaticPidGains()
        {
            EXPECT_CALL(interfaceMock, PhaseCurrentsReady(::testing::_, ::testing::_))
                .WillOnce([this](auto, const auto& onDone)
                    {
                        interfaceMock.StorePhaseCurrentsCallback(onDone);
                    });

            foc.emplace(interfaceMock, encoderMock, focMock);
        }

        ::testing::StrictMock<foc::FieldOrientedControllerInterfaceMock> interfaceMock;
        ::testing::StrictMock<foc::EncoderMock> encoderMock;
        ::testing::StrictMock<foc::FieldOrientedControllerMock> focMock;
        std::optional<foc::WithAutomaticCurrentPidGains<foc::TorqueControllerImpl>> foc;
    };
}

TEST_F(TestTorqueControllerWithAutomaticPidGains, calculates_pid_gains_from_resistance_and_inductance)
{
    foc::Volts Vdc{ 12.0f };
    foc::Ohm resistance{ 0.5f };
    foc::MilliHenry inductance{ 1.0f };
    float nyquistFactor = 15.0f;

    float expectedWc = (10000.0f / 15.0f) * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = 0.001f * expectedWc;
    float expectedKi = 0.5f * expectedWc;

    EXPECT_CALL(interfaceMock, BaseFrequency())
        .WillOnce(::testing::Return(hal::Hertz{ 10000 }));
    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsNear(expectedKp, expectedKi)));

    foc->SetPidBasedOnResistanceAndInductance(Vdc, resistance, inductance, nyquistFactor);
}

TEST_F(TestTorqueControllerWithAutomaticPidGains, calculates_pid_gains_with_different_nyquist_factor)
{
    foc::Volts Vdc{ 24.0f };
    foc::Ohm resistance{ 1.0f };
    foc::MilliHenry inductance{ 2.0f };
    float nyquistFactor = 10.0f;

    float expectedWc = (10000.0f / 10.0f) * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = 0.002f * expectedWc;
    float expectedKi = 1.0f * expectedWc;

    EXPECT_CALL(interfaceMock, BaseFrequency())
        .WillOnce(::testing::Return(hal::Hertz{ 10000 }));
    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsNear(expectedKp, expectedKi)));

    foc->SetPidBasedOnResistanceAndInductance(Vdc, resistance, inductance, nyquistFactor);
}

TEST_F(TestTorqueControllerWithAutomaticPidGains, calculates_pid_gains_with_different_base_frequency)
{
    foc::Volts Vdc{ 12.0f };
    foc::Ohm resistance{ 0.5f };
    foc::MilliHenry inductance{ 1.0f };
    float nyquistFactor = 15.0f;

    float baseFreq = 20000.0f;
    float expectedWc = (baseFreq / 15.0f) * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = 0.001f * expectedWc;
    float expectedKi = 0.5f * expectedWc;

    EXPECT_CALL(interfaceMock, BaseFrequency())
        .WillOnce(::testing::Return(hal::Hertz{ 20000 }));
    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsNear(expectedKp, expectedKi)));

    foc->SetPidBasedOnResistanceAndInductance(Vdc, resistance, inductance, nyquistFactor);
}

TEST_F(TestTorqueControllerWithAutomaticPidGains, calculates_pid_gains_with_small_inductance)
{
    foc::Volts Vdc{ 12.0f };
    foc::Ohm resistance{ 2.0f };
    foc::MilliHenry inductance{ 0.1f };
    float nyquistFactor = 15.0f;

    float expectedWc = (10000.0f / 15.0f) * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = 0.0001f * expectedWc;
    float expectedKi = 2.0f * expectedWc;

    EXPECT_CALL(interfaceMock, BaseFrequency())
        .WillOnce(::testing::Return(hal::Hertz{ 10000 }));
    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsNear(expectedKp, expectedKi)));

    foc->SetPidBasedOnResistanceAndInductance(Vdc, resistance, inductance, nyquistFactor);
}

TEST_F(TestTorqueControllerWithAutomaticPidGains, calculates_pid_gains_with_high_resistance)
{
    foc::Volts Vdc{ 48.0f };
    foc::Ohm resistance{ 5.0f };
    foc::MilliHenry inductance{ 3.0f };
    float nyquistFactor = 12.0f;

    float expectedWc = (10000.0f / 12.0f) * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = 0.003f * expectedWc;
    float expectedKi = 5.0f * expectedWc;

    EXPECT_CALL(interfaceMock, BaseFrequency())
        .WillOnce(::testing::Return(hal::Hertz{ 10000 }));
    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsNear(expectedKp, expectedKi)));

    foc->SetPidBasedOnResistanceAndInductance(Vdc, resistance, inductance, nyquistFactor);
}

TEST_F(TestTorqueControllerWithAutomaticPidGains, inherits_basic_torque_controller_functionality)
{
    EXPECT_CALL(focMock, Reset());
    EXPECT_CALL(interfaceMock, Start());
    foc->Enable();

    EXPECT_TRUE(foc->IsRunning());

    EXPECT_CALL(interfaceMock, Stop());
    foc->Disable();

    EXPECT_FALSE(foc->IsRunning());
}

TEST_F(TestTorqueControllerWithAutomaticPidGains, can_set_manual_tunings_after_automatic_calculation)
{
    foc::Volts Vdc{ 12.0f };
    foc::Ohm resistance{ 0.5f };
    foc::MilliHenry inductance{ 1.0f };
    float nyquistFactor = 15.0f;

    float expectedWc = (10000.0f / 15.0f) * 2.0f * std::numbers::pi_v<float>;
    float expectedKp = 0.001f * expectedWc;
    float expectedKi = 0.5f * expectedWc;

    EXPECT_CALL(interfaceMock, BaseFrequency())
        .WillOnce(::testing::Return(hal::Hertz{ 10000 }));
    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsNear(expectedKp, expectedKi)));
    foc->SetPidBasedOnResistanceAndInductance(Vdc, resistance, inductance, nyquistFactor);

    controllers::PidTunings<float> customDTunings{ 10.0f, 20.0f, 0.5f };
    controllers::PidTunings<float> customQTunings{ 15.0f, 25.0f, 1.0f };

    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), ::testing::_));
    foc->SetCurrentTunings(Vdc, { customDTunings, customQTunings });
}
