#include "core/foc/interfaces/Units.hpp"
#include <gtest/gtest.h>

namespace
{
    using RevPerSecond = infra::Quantity<foc::unit::RevPerSecond, float>;

    static_assert(foc::unit::Radians::Factor::n == 1 && foc::unit::Radians::Factor::d == 1);
    static_assert(foc::unit::RevPerSecond::Factor::n == 1 && foc::unit::RevPerSecond::Factor::d == 1);
    static_assert(foc::unit::RevPerMinute::Factor::n == 1 && foc::unit::RevPerMinute::Factor::d == 60);

    TEST(TestUnits, one_revolution_per_second_is_sixty_revolutions_per_minute)
    {
        const foc::RevPerMinute rpm{ RevPerSecond{ 1.0f } };

        EXPECT_NEAR(rpm.Value(), 60.0f, 1e-4f);
    }

    TEST(TestUnits, revolutions_per_minute_convert_back_to_revolutions_per_second)
    {
        const RevPerSecond rps{ foc::RevPerMinute{ 3000.0f } };

        EXPECT_NEAR(rps.Value(), 50.0f, 1e-4f);
    }
}
