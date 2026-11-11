#include "tools/virtual_motor/platform/PlantDisturbances.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cmath>
#include <limits>

namespace
{
    using virtual_motor::port::ExternalTorque;
    using virtual_motor::port::RefusalReason;
    using virtual_motor::port::TorqueForm;
    namespace differs = virtual_motor::port::differs;

    MATCHER_P(IsNear, expected, "")
    {
        return std::abs(arg.Value() - expected) < 1e-5f;
    }

    class DisturbanceInputsMock
        : public virtual_motor::DisturbanceInputs
    {
    public:
        MOCK_METHOD(void, SetLoadTorque, (foc::NewtonMeter torque), (override));
        MOCK_METHOD(void, SetExternalTorque, (foc::NewtonMeter torque), (override));
        MOCK_METHOD(void, SetCurrentNoise, (foc::Ampere sigma), (override));
        MOCK_METHOD(void, SetEncoderNoise, (foc::Radians sigma), (override));
    };

    class TestPlantDisturbances
        : public testing::Test
    {
    public:
        void ExpectExternalTorques(std::initializer_list<float> torques)
        {
            testing::InSequence sequence;
            for (auto torque : torques)
                EXPECT_CALL(inputs, SetExternalTorque(IsNear(torque)));
            for (std::size_t period = 0; period != torques.size(); ++period)
                disturbances.Apply();
        }

        static constexpr uint32_t controlRateHz{ 1000 };
        static constexpr float nan{ std::numeric_limits<float>::quiet_NaN() };

        testing::StrictMock<DisturbanceInputsMock> inputs;
        virtual_motor::PlantDisturbances disturbances{ inputs, { foc::NewtonMeter{ 0.01f }, foc::Ampere{ 0.02f }, foc::Radians{ 0.001f } }, controlRateHz };
    };
}

TEST_F(TestPlantDisturbances, starts_from_the_plant_description_with_nothing_changed)
{
    const auto changes = disturbances.InForce();

    EXPECT_FLOAT_EQ(changes.loadTorque, 0.01f);
    EXPECT_EQ(changes.externalTorque.form, TorqueForm::none);
    EXPECT_FLOAT_EQ(changes.currentNoise, 0.02f);
    EXPECT_FLOAT_EQ(changes.encoderNoise, 0.001f);
    EXPECT_EQ(changes.differs, 0);

    disturbances.Apply();
}

TEST_F(TestPlantDisturbances, load_torque_and_noise_apply_at_once_and_are_listed)
{
    EXPECT_CALL(inputs, SetLoadTorque(IsNear(0.2f)));
    EXPECT_CALL(inputs, SetCurrentNoise(IsNear(0.05f)));
    EXPECT_CALL(inputs, SetEncoderNoise(IsNear(0.004f)));

    EXPECT_FALSE(disturbances.SetLoadTorque(0.2f).has_value());
    EXPECT_FALSE(disturbances.SetCurrentNoise(0.05f).has_value());
    EXPECT_FALSE(disturbances.SetEncoderNoise(0.004f).has_value());

    const auto changes = disturbances.InForce();
    EXPECT_FLOAT_EQ(changes.loadTorque, 0.2f);
    EXPECT_FLOAT_EQ(changes.currentNoise, 0.05f);
    EXPECT_FLOAT_EQ(changes.encoderNoise, 0.004f);
    EXPECT_EQ(changes.differs, differs::loadTorque | differs::currentNoise | differs::encoderNoise);
}

TEST_F(TestPlantDisturbances, a_constant_external_torque_acts_every_control_period)
{
    EXPECT_FALSE(disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::constant, { -0.3f } }).has_value());

    ExpectExternalTorques({ -0.3f, -0.3f, -0.3f });
    EXPECT_EQ(disturbances.InForce().differs, differs::externalTorque);
}

TEST_F(TestPlantDisturbances, a_step_holds_its_initial_value_until_the_delay_has_passed)
{
    disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::step, { 0.1f, 0.5f, 0.002f } });

    ExpectExternalTorques({ 0.1f, 0.1f, 0.5f, 0.5f });
}

TEST_F(TestPlantDisturbances, a_ramp_moves_linearly_over_its_duration_then_holds_its_end)
{
    disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::ramp, { 0.0f, 0.4f, 0.004f } });

    ExpectExternalTorques({ 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.4f });
}

TEST_F(TestPlantDisturbances, a_sine_adds_its_amplitude_to_its_offset)
{
    disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::sine, { 0.1f, 0.2f, 100.0f } });

    ExpectExternalTorques({ 0.1f, 0.1f + 0.2f * std::sin(0.2f * std::numbers::pi_v<float>), 0.1f + 0.2f * std::sin(0.4f * std::numbers::pi_v<float>) });
}

TEST_F(TestPlantDisturbances, a_new_external_torque_starts_its_own_time)
{
    disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::ramp, { 0.0f, 0.4f, 0.004f } });
    ExpectExternalTorques({ 0.0f, 0.1f });

    disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::step, { 0.2f, 0.3f, 0.001f } });
    ExpectExternalTorques({ 0.2f, 0.3f });
}

TEST_F(TestPlantDisturbances, removing_the_external_torque_leaves_the_shaft_free_of_it)
{
    disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::constant, { 0.3f } });
    ExpectExternalTorques({ 0.3f });

    EXPECT_CALL(inputs, SetExternalTorque(IsNear(0.0f)));
    EXPECT_FALSE(disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::none }).has_value());
    disturbances.Apply();

    EXPECT_EQ(disturbances.InForce().differs, 0);
}

TEST_F(TestPlantDisturbances, clear_all_returns_the_plant_to_its_description)
{
    EXPECT_CALL(inputs, SetLoadTorque(testing::_));
    EXPECT_CALL(inputs, SetCurrentNoise(testing::_));
    EXPECT_CALL(inputs, SetEncoderNoise(testing::_));
    EXPECT_CALL(inputs, SetExternalTorque(testing::_));
    disturbances.SetLoadTorque(0.2f);
    disturbances.SetCurrentNoise(0.05f);
    disturbances.SetEncoderNoise(0.004f);
    disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::constant, { 0.3f } });
    disturbances.Apply();
    testing::Mock::VerifyAndClearExpectations(&inputs);

    EXPECT_CALL(inputs, SetLoadTorque(IsNear(0.01f)));
    EXPECT_CALL(inputs, SetCurrentNoise(IsNear(0.02f)));
    EXPECT_CALL(inputs, SetEncoderNoise(IsNear(0.001f)));
    EXPECT_CALL(inputs, SetExternalTorque(IsNear(0.0f)));
    disturbances.ClearAll();
    disturbances.Apply();

    EXPECT_EQ(disturbances.InForce().differs, 0);
}

TEST_F(TestPlantDisturbances, refuses_values_out_of_range_and_leaves_the_plant_as_it_was)
{
    EXPECT_EQ(disturbances.SetLoadTorque(nan), RefusalReason::outOfRange);
    EXPECT_EQ(disturbances.SetCurrentNoise(-0.01f), RefusalReason::outOfRange);
    EXPECT_EQ(disturbances.SetEncoderNoise(std::numeric_limits<float>::infinity()), RefusalReason::outOfRange);
    EXPECT_EQ(disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::constant, { nan } }), RefusalReason::outOfRange);
    EXPECT_EQ(disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::step, { 0.0f, 0.1f, -0.001f } }), RefusalReason::outOfRange);
    EXPECT_EQ(disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::ramp, { 0.0f, 0.1f, 0.0f } }), RefusalReason::outOfRange);
    EXPECT_EQ(disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::sine, { 0.0f, 0.1f, 0.0f } }), RefusalReason::outOfRange);
    EXPECT_EQ(disturbances.SetExternalTorque(ExternalTorque{ TorqueForm::sine, { 0.0f, 0.1f, 101.0f } }), RefusalReason::outOfRange);
    EXPECT_EQ(disturbances.SetExternalTorque(ExternalTorque{ static_cast<TorqueForm>(9) }), RefusalReason::unknownForm);

    disturbances.Apply();
    EXPECT_EQ(disturbances.InForce().differs, 0);
}

TEST_F(TestPlantDisturbances, a_profile_step_is_checked_and_applied_as_the_change_it_names)
{
    using virtual_motor::port::MessageType;
    using virtual_motor::port::ProfileStep;

    EXPECT_FALSE(disturbances.Check(ProfileStep{ 0, MessageType::currentNoise, 0.04f, {} }).has_value());
    EXPECT_EQ(disturbances.Check(ProfileStep{ 0, MessageType::encoderNoise, -1.0f, {} }), RefusalReason::outOfRange);
    EXPECT_EQ(disturbances.Check(ProfileStep{ 0, MessageType::externalTorque, 0.0f, ExternalTorque{ TorqueForm::ramp, { 0.0f, 1.0f, 0.0f } } }), RefusalReason::outOfRange);
    EXPECT_EQ(disturbances.Check(ProfileStep{ 0, MessageType::pause, 0.0f, {} }), RefusalReason::outOfRange);

    EXPECT_CALL(inputs, SetCurrentNoise(IsNear(0.04f)));
    disturbances.Apply(ProfileStep{ 0, MessageType::currentNoise, 0.04f, {} });
    disturbances.Apply(ProfileStep{ 0, MessageType::externalTorque, 0.0f, ExternalTorque{ TorqueForm::constant, { 0.2f } } });
    ExpectExternalTorques({ 0.2f });
}
