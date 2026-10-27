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
        int measureCalls{ 0 };
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
            ++capture.measureCalls;
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
    EXPECT_NEAR(measured->a.Value(), 0.0f, 1e-3f);
    EXPECT_NEAR(measured->b.Value(), 512.0f, 1e-3f);
    EXPECT_NEAR(measured->c.Value(), -512.0f, 1e-3f);
}

TEST_F(TestAdcPhaseCurrentMeasurement, a_conversion_carrying_more_samples_than_phases_uses_the_first_three)
{
    Start();

    const std::array<uint16_t, 5> withExtras{ { 2048, 3072, 1024, 4095, 0 } };
    capture.onSamples(infra::MakeRange(withExtras));

    ASSERT_TRUE(measured.has_value());
    EXPECT_NEAR(measured->b.Value(), 512.0f, 1e-3f);
}

TEST_F(TestAdcPhaseCurrentMeasurement, stopping_reaches_the_converter)
{
    subject.Stop();

    EXPECT_TRUE(capture.stopped);
}

TEST_F(TestAdcPhaseCurrentMeasurement, the_converter_callback_is_handed_over_once_however_often_the_target_changes)
{
    subject.Measure([](foc::Ampere, foc::Ampere, foc::Ampere) {});
    subject.Measure([](foc::Ampere, foc::Ampere, foc::Ampere) {});
    subject.Measure([](foc::Ampere, foc::Ampere, foc::Ampere) {});

    EXPECT_EQ(capture.measureCalls, 1);
}

TEST_F(TestAdcPhaseCurrentMeasurement, a_later_target_receives_the_conversions_in_place_of_the_earlier_one)
{
    int first = 0;
    int second = 0;
    const std::array<uint16_t, 3> samples{ 1, 2, 3 };

    subject.Measure([&first](foc::Ampere, foc::Ampere, foc::Ampere)
        {
            ++first;
        });
    subject.Measure([&second](foc::Ampere, foc::Ampere, foc::Ampere)
        {
            ++second;
        });
    capture.onSamples(infra::MakeRange(samples));

    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 1);
}

TEST_F(TestAdcPhaseCurrentMeasurement, measuring_again_after_a_stop_hands_the_converter_a_new_callback)
{
    subject.Measure([](foc::Ampere, foc::Ampere, foc::Ampere) {});
    subject.Stop();
    subject.Measure([](foc::Ampere, foc::Ampere, foc::Ampere) {});

    EXPECT_EQ(capture.measureCalls, 2);
}
