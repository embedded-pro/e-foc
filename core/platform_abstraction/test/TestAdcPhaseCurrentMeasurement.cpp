#include "core/platform_abstraction/AdcPhaseCurrentMeasurement.hpp"
#include <array>
#include <gmock/gmock.h>
#include <optional>

namespace
{
    using namespace testing;

    struct Capture
    {
        infra::Function<void(hal::AdcMultiChannel::Samples)> onSamples;
        bool stopped{ false };
    };

    class CapturingAdc
        : public hal::AdcMultiChannel
    {
    public:
        explicit CapturingAdc(Capture& capture)
            : capture(capture)
        {}

        void Measure(const infra::Function<void(Samples)>& onDone) override
        {
            capture.onSamples = onDone;
        }

        void Stop() override
        {
            capture.stopped = true;
        }

    private:
        Capture& capture;
    };

    struct Measured
    {
        foc::Ampere a;
        foc::Ampere b;
        foc::Ampere c;
    };

    class TestAdcPhaseCurrentMeasurement
        : public ::testing::Test
    {
    public:
        static constexpr float slope{ 0.5f };
        static constexpr float offset{ -1024.0f };

        Capture capture;
        application::AdcPhaseCurrentMeasurementImpl<CapturingAdc> subject{ slope, offset, capture };

        std::optional<Measured> measured;

        void Start()
        {
            subject.Measure([this](foc::Ampere a, foc::Ampere b, foc::Ampere c)
                {
                    measured = Measured{ a, b, c };
                });
        }
    };
}

TEST_F(TestAdcPhaseCurrentMeasurement, a_full_conversion_is_scaled_and_offset_onto_amperes)
{
    Start();

    const std::array<uint16_t, 3> samples{ { 2048, 3072, 1024 } };
    capture.onSamples(infra::MakeRange(samples));

    ASSERT_TRUE(measured.has_value());
    EXPECT_FLOAT_EQ(measured->a.Value(), 0.0f);
    EXPECT_FLOAT_EQ(measured->b.Value(), 512.0f);
    EXPECT_FLOAT_EQ(measured->c.Value(), -512.0f);
}

TEST_F(TestAdcPhaseCurrentMeasurement, a_conversion_that_drains_fewer_samples_than_phases_is_discarded)
{
    Start();

    const std::array<uint16_t, 2> tooFew{ { 2048, 3072 } };
    capture.onSamples(infra::MakeRange(tooFew));

    EXPECT_FALSE(measured.has_value());
}

TEST_F(TestAdcPhaseCurrentMeasurement, a_conversion_carrying_more_samples_than_phases_uses_the_first_three)
{
    Start();

    const std::array<uint16_t, 5> withExtras{ { 2048, 3072, 1024, 4095, 0 } };
    capture.onSamples(infra::MakeRange(withExtras));

    ASSERT_TRUE(measured.has_value());
    EXPECT_FLOAT_EQ(measured->b.Value(), 512.0f);
}

TEST_F(TestAdcPhaseCurrentMeasurement, stopping_reaches_the_converter)
{
    subject.Stop();

    EXPECT_TRUE(capture.stopped);
}
