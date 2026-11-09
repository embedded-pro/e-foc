#include "core/foc/model/MechanicalPlant.hpp"
#include "core/foc/model/SimulatedEncoder.hpp"
#include "gtest/gtest.h"

namespace
{
    class TestSimulatedEncoder
        : public testing::Test
    {
    public:
        foc::MechanicalPlant shaft{ foc::KilogramMeterSquared{ 1.0e-5f }, foc::NewtonMeterSecondPerRadian{ 0.0f }, 4 };
        foc::SimulatedEncoder encoder{ shaft };
    };
}

TEST_F(TestSimulatedEncoder, without_an_index_setting_the_reading_moves_the_rotor)
{
    encoder.Set(foc::Radians{ 1.0f });

    EXPECT_NEAR(1.0f, shaft.Angle().Value(), 1e-6f);
    EXPECT_NEAR(1.0f, encoder.Read().Value(), 1e-6f);

    encoder.SetZero();
    EXPECT_NEAR(0.0f, shaft.Angle().Value(), 1e-6f);
}

TEST_F(TestSimulatedEncoder, with_an_index_setting_the_reading_keeps_the_rotor_where_it_is)
{
    shaft.Set(foc::Radians{ 0.5f });
    encoder.SetIndex(foc::Radians{ 0.0f }, 1);
    ASSERT_TRUE(encoder.HasIndex());

    encoder.Set(foc::Radians{ 2.0f });

    EXPECT_NEAR(0.5f, shaft.Angle().Value(), 1e-6f);
    EXPECT_NEAR(2.0f, encoder.Read().Value(), 1e-4f);
}

TEST_F(TestSimulatedEncoder, a_stuck_encoder_keeps_reporting_the_angle_it_stuck_at)
{
    shaft.Set(foc::Radians{ 0.3f });
    encoder.SetStuck(true);
    shaft.Set(foc::Radians{ 1.3f });

    EXPECT_NEAR(0.3f, encoder.Read().Value(), 1e-6f);
}

TEST_F(TestSimulatedEncoder, the_index_is_asserted_only_at_its_angle)
{
    encoder.SetResolution(4000);
    encoder.SetIndex(foc::Radians{ 1.0f }, 1);

    shaft.Set(foc::Radians{ 1.0f });
    EXPECT_TRUE(encoder.IndexAsserted());

    shaft.Set(foc::Radians{ 2.0f });
    EXPECT_FALSE(encoder.IndexAsserted());
}
