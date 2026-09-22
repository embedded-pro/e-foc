#include "core/foc/math/TorqueConstant.hpp"
#include <gtest/gtest.h>

TEST(TorqueConstantTest, is_three_halves_of_pole_pairs_times_flux_linkage)
{
    EXPECT_NEAR(foc::TorqueConstantFor(4, foc::Weber{ 0.0064f }).Value(), 0.0384f, 1e-6f);
    EXPECT_NEAR(foc::TorqueConstantFor(7, foc::Weber{ 0.01f }).Value(), 0.105f, 1e-6f);
}

TEST(TorqueConstantTest, vanishes_without_pole_pairs_or_flux)
{
    EXPECT_FLOAT_EQ(foc::TorqueConstantFor(0, foc::Weber{ 0.0064f }).Value(), 0.0f);
    EXPECT_FLOAT_EQ(foc::TorqueConstantFor(4, foc::Weber{ 0.0f }).Value(), 0.0f);
}
