#include "core/foc/math/FiniteGuard.hpp"
#include <gmock/gmock.h>
#include <limits>

TEST(TestFiniteGuard, a_finite_value_is_a_finite_value)
{
    EXPECT_TRUE(foc::IsFiniteValue(0.0f));
    EXPECT_TRUE(foc::IsFiniteValue(-1.0f));
    EXPECT_TRUE(foc::IsFiniteValue(3.4e38f));
}

TEST(TestFiniteGuard, nan_and_infinity_are_not_finite)
{
    EXPECT_FALSE(foc::IsFiniteValue(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(foc::IsFiniteValue(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(foc::IsFiniteValue(-std::numeric_limits<float>::infinity()));
}
