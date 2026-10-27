#include "core/platform_abstraction/QuadratureEncoderDecorator.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float pi = std::numbers::pi_v<float>;

    class FakeQuadratureEncoder
        : public hal::SynchronousQuadratureEncoder
    {
    public:
        explicit FakeQuadratureEncoder(const uint32_t& position)
            : position(position)
        {}

        uint32_t Position() override
        {
            return position;
        }

        uint32_t Resolution() override
        {
            return 0;
        }

        MotionDirection Direction() override
        {
            return MotionDirection::forward;
        }

        uint32_t Speed() override
        {
            return 0;
        }

    private:
        const uint32_t& position;
    };

    class TestQuadratureEncoderDecorator
        : public ::testing::Test
    {
    public:
        float AngleAt(uint32_t count)
        {
            position = count;
            return encoder.Read().Value();
        }

        uint32_t position{ 0 };
        application::QuadratureEncoderDecoratorImpl<FakeQuadratureEncoder> encoder{ 4000, position };
    };
}

TEST_F(TestQuadratureEncoderDecorator, the_first_half_turn_reads_as_a_positive_angle)
{
    EXPECT_NEAR(AngleAt(0), 0.0f, 1e-6f);
    EXPECT_NEAR(AngleAt(1000), pi / 2.0f, 1e-6f);
    EXPECT_NEAR(AngleAt(1999), pi - 2.0f * pi / 4000.0f, 1e-5f);
}

TEST_F(TestQuadratureEncoderDecorator, the_second_half_turn_wraps_to_a_negative_angle)
{
    EXPECT_NEAR(AngleAt(2000), -pi, 1e-6f);
    EXPECT_NEAR(AngleAt(3000), -pi / 2.0f, 1e-6f);
    EXPECT_NEAR(AngleAt(3999), -2.0f * pi / 4000.0f, 1e-5f);
}

TEST_F(TestQuadratureEncoderDecorator, a_zero_resolution_reads_zero_rather_than_not_a_number)
{
    application::QuadratureEncoderDecoratorImpl<FakeQuadratureEncoder> unconfigured{ 0, position };

    EXPECT_NEAR(unconfigured.Read().Value(), 0.0f, 1e-9f);
}
