#include "tools/virtual_motor/platform/PlantInstruments.hpp"
#include "tools/virtual_motor/platform/test/InstrumentReportsMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace virtual_motor::port;

    class DisturbanceInputsMock
        : public virtual_motor::DisturbanceInputs
    {
    public:
        MOCK_METHOD(void, SetLoadTorque, (foc::NewtonMeter torque), (override));
        MOCK_METHOD(void, SetExternalTorque, (foc::NewtonMeter torque), (override));
        MOCK_METHOD(void, SetCurrentNoise, (foc::Ampere sigma), (override));
        MOCK_METHOD(void, SetEncoderNoise, (foc::Radians sigma), (override));
    };

    class TestPlantInstruments
        : public testing::Test
    {
    public:
        TestPlantInstruments()
        {
            instruments.Attach(reports);
        }

        void Observe(uint64_t from, uint64_t to)
        {
            for (auto period = from; period != to; ++period)
                instruments.Observe(period, Sample{}, 0, false);
        }

        testing::StrictMock<DisturbanceInputsMock> inputs;
        virtual_motor::PlantDisturbances disturbances{ inputs, { foc::NewtonMeter{ 0.0f }, foc::Ampere{ 0.0f }, foc::Radians{ 0.0f } }, 1000 };
        testing::StrictMock<virtual_motor::InstrumentReportsMock> reports;
        virtual_motor::PlantInstruments instruments{ disturbances };
    };
}

TEST_F(TestPlantInstruments, nothing_is_active_until_asked)
{
    EXPECT_FALSE(instruments.Active());
}

TEST_F(TestPlantInstruments, a_stream_reports_a_window_every_ten_periods)
{
    instruments.Stream(true);
    EXPECT_TRUE(instruments.Active());

    EXPECT_CALL(reports, ReportWindow(testing::Field(&Window::firstPeriod, 40u)));
    Observe(40, 55);

    instruments.Stream(false);
    EXPECT_FALSE(instruments.Active());
}

TEST_F(TestPlantInstruments, a_capture_and_a_profile_keep_the_instruments_active)
{
    EXPECT_CALL(reports, ReportCaptureEvent(testing::_));
    EXPECT_FALSE(instruments.Capture(CaptureRequest{ Trigger{ TriggerKind::rising, 0, 1.0f }, 0, 10 }, 0).has_value());
    EXPECT_TRUE(instruments.Active());

    EXPECT_CALL(reports, ReportProfileState(testing::_));
    EXPECT_FALSE(instruments.RunProfile(ProfileRequest{ Trigger{ TriggerKind::rising, 0, 1.0f }, {} }).has_value());

    EXPECT_CALL(reports, ReportCaptureEvent(testing::Field(&CaptureEvent::event, CaptureEventKind::cancelled)));
    instruments.CancelCapture(3);
    EXPECT_TRUE(instruments.Active());
}

TEST_F(TestPlantInstruments, detaching_stops_the_stream_cancels_the_capture_and_abandons_the_profile)
{
    instruments.Stream(true);
    EXPECT_CALL(reports, ReportCaptureEvent(testing::_));
    instruments.Capture(CaptureRequest{ Trigger{ TriggerKind::rising, 0, 1.0f }, 0, 10 }, 0);
    EXPECT_CALL(reports, ReportProfileState(testing::_));
    instruments.RunProfile(ProfileRequest{ Trigger{ TriggerKind::rising, 0, 1.0f }, {} });

    EXPECT_CALL(reports, ReportCaptureEvent(testing::Field(&CaptureEvent::event, CaptureEventKind::cancelled)));
    EXPECT_CALL(reports, ReportProfileState(testing::Field(&ProfileState::state, ProfileStateKind::stopped)));
    instruments.Detach(7);

    EXPECT_FALSE(instruments.Active());
    Observe(7, 100);
}
