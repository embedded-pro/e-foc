#include "core/foc/interfaces/IsrCallback.hpp"
#include "gmock/gmock.h"

namespace
{
    using namespace testing;

    class TestIsrCallback
        : public Test
    {
    public:
        foc::IsrCallback<void(int)> callback;
        int total{ 0 };
    };

    TEST_F(TestIsrCallback, a_callback_that_was_never_set_is_skipped)
    {
        callback(5);

        EXPECT_EQ(total, 0);
        EXPECT_FALSE(callback.IsSet());
    }

    TEST_F(TestIsrCallback, a_set_callback_is_invoked_with_the_arguments)
    {
        callback.Set([this](int value)
            {
                total += value;
            });

        callback(5);
        callback(2);

        EXPECT_EQ(total, 7);
        EXPECT_TRUE(callback.IsSet());
    }

    TEST_F(TestIsrCallback, a_replacement_takes_over_from_the_previous_callback)
    {
        callback.Set([this](int)
            {
                total += 1;
            });
        callback.Set([this](int)
            {
                total += 100;
            });

        callback(0);

        EXPECT_EQ(total, 100);
    }

    TEST_F(TestIsrCallback, a_cleared_callback_is_skipped)
    {
        callback.Set([this](int)
            {
                ++total;
            });
        callback.Clear();

        callback(0);

        EXPECT_EQ(total, 0);
        EXPECT_FALSE(callback.IsSet());
    }

    TEST_F(TestIsrCallback, a_callback_can_clear_itself_while_it_runs)
    {
        callback.Set([this](int)
            {
                ++total;
                callback.Clear();
                ++total;
            });

        callback(0);
        callback(0);

        EXPECT_EQ(total, 2);
    }

    TEST_F(TestIsrCallback, a_callback_can_replace_itself_while_it_runs)
    {
        callback.Set([this](int)
            {
                total += 1;
                callback.Set([this](int)
                    {
                        total += 10;
                    });
                total += 1;
            });

        callback(0);
        callback(0);

        EXPECT_EQ(total, 12);
    }

    TEST_F(TestIsrCallback, repeated_replacement_alternates_between_the_slots_without_losing_the_latest)
    {
        for (int generation = 1; generation != 8; ++generation)
        {
            callback.Set([this, generation](int)
                {
                    total = generation;
                });
            callback(0);

            EXPECT_EQ(total, generation);
        }
    }
}
