#include "core/services/non_volatile_memory/test_doubles/NonVolatileMemoryMock.hpp"
#include "core/state_machine/RecalibrationStore.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>
#include <vector>

namespace
{
    using namespace testing;
    using namespace std::chrono_literals;

    state_machine::MotorModel Commissioned()
    {
        return state_machine::MotorModel{ foc::Ohm{ 0.36f }, foc::MilliHenry{ 0.2f }, foc::NewtonMeterSecondSquared{ 1.0e-5f }, foc::NewtonMeterSecondPerRadian{ 4.0e-6f } };
    }

    state_machine::StoredModel LearnedInertia(float inertia)
    {
        return state_machine::StoredModel{ std::nullopt, foc::NewtonMeterSecondSquared{ inertia }, std::nullopt };
    }

    class TestRecalibrationStore
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Load(const services::LearnedModelData& stored, services::NvmStatus status = services::NvmStatus::Ok)
        {
            EXPECT_CALL(nvm, LoadLearnedModel(_, _)).WillOnce(Invoke([this, stored, status](services::LearnedModelData& model, const infra::Function<void(services::NvmStatus)>& onDone)
                {
                    EXPECT_TRUE(activity.InFlight());
                    model = stored;
                    onDone(status);
                }));
        }

        void ExpectStores()
        {
            EXPECT_CALL(nvm, SaveLearnedModel(_, _)).WillRepeatedly(Invoke([this](const services::LearnedModelData& model, const infra::Function<void(services::NvmStatus)>& onDone)
                {
                    EXPECT_TRUE(activity.InFlight());
                    written.push_back(model);
                    onDone(saveStatus);
                }));
        }

        void Started(const services::LearnedModelData& stored = {})
        {
            Load(stored);
            ExpectStores();
            store.Start([this]()
                {
                    loaded = true;
                });
        }

        services::LearnedModelData StoredInertia(float inertia)
        {
            return written.empty() ? services::LearnedModelData{} : services::LearnedModelData{ written.front().commissioning, std::nullopt, inertia, std::nullopt };
        }

        StrictMock<services::NonVolatileMemoryMock> nvm;
        application::NvmActivity activity;
        state_machine::RecalibrationStore store{ nvm, activity };
        services::NvmStatus saveStatus{ services::NvmStatus::Ok };
        std::vector<services::LearnedModelData> written;
        bool loaded{ false };
    };
}

TEST_F(TestRecalibrationStore, a_stored_model_learned_from_the_commissioned_one_is_restored)
{
    Started();
    store.Store(LearnedInertia(2.0e-5f), Commissioned());
    ASSERT_EQ(1u, written.size());

    state_machine::RecalibrationStore afterRestart{ nvm, activity };
    Load(written.back());
    afterRestart.Start([]() {});

    EXPECT_EQ(LearnedInertia(2.0e-5f), afterRestart.Restored(Commissioned()));
    EXPECT_TRUE(loaded);
}

TEST_F(TestRecalibrationStore, a_stored_model_learned_from_another_commissioning_is_not_restored)
{
    Started();
    store.Store(LearnedInertia(2.0e-5f), Commissioned());

    state_machine::RecalibrationStore afterRestart{ nvm, activity };
    Load(written.back());
    afterRestart.Start([]() {});
    auto recommissioned = Commissioned();
    recommissioned.inertia = foc::NewtonMeterSecondSquared{ 1.1e-5f };

    EXPECT_EQ(state_machine::StoredModel{}, afterRestart.Restored(recommissioned));
}

TEST_F(TestRecalibrationStore, an_unreadable_record_restores_nothing)
{
    Load(services::LearnedModelData{ 0, 1.0f, 1.0f, 1.0f }, services::NvmStatus::InvalidData);
    store.Start([this]()
        {
            loaded = true;
        });

    EXPECT_TRUE(loaded);
    EXPECT_EQ(state_machine::StoredModel{}, store.Restored(Commissioned()));
}

TEST_F(TestRecalibrationStore, a_busy_device_is_read_again_10_ms_later)
{
    Load({}, services::NvmStatus::Busy);
    store.Start([this]()
        {
            loaded = true;
        });
    EXPECT_FALSE(loaded);

    Load({});
    ForwardTime(state_machine::RecalibrationStore::busyRetry);

    EXPECT_TRUE(loaded);
}

TEST_F(TestRecalibrationStore, nothing_is_stored_before_the_record_has_been_read)
{
    store.Store(LearnedInertia(2.0e-5f), Commissioned());

    Started();

    ASSERT_EQ(1u, written.size());
    EXPECT_EQ(2.0e-5f, written.front().inertia);
    EXPECT_FALSE(written.front().inductance.has_value());
}

TEST_F(TestRecalibrationStore, a_change_within_15_minutes_of_a_store_waits_for_the_first_stop_after_them)
{
    Started();
    store.Store(LearnedInertia(2.0e-5f), Commissioned());

    ForwardTime(10min);
    store.Store(LearnedInertia(3.0e-5f), Commissioned());
    ForwardTime(5min);
    EXPECT_EQ(1u, written.size());

    store.StoreIfDue();

    ASSERT_EQ(2u, written.size());
    EXPECT_EQ(StoredInertia(3.0e-5f), written.back());
    EXPECT_EQ(2, store.Stores());
}

TEST_F(TestRecalibrationStore, only_the_latest_waiting_change_is_stored)
{
    Started();
    store.Store(LearnedInertia(2.0e-5f), Commissioned());
    ForwardTime(1min);
    store.Store(LearnedInertia(3.0e-5f), Commissioned());
    store.Store(LearnedInertia(4.0e-5f), Commissioned());

    ForwardTime(15min);
    store.StoreIfDue();
    store.StoreIfDue();

    ASSERT_EQ(2u, written.size());
    EXPECT_EQ(StoredInertia(4.0e-5f), written.back());
}

TEST_F(TestRecalibrationStore, a_model_equal_to_the_stored_one_is_not_written_again)
{
    Started();
    store.Store(LearnedInertia(2.0e-5f), Commissioned());
    ForwardTime(20min);

    store.Store(LearnedInertia(2.0e-5f), Commissioned());

    EXPECT_EQ(1u, written.size());
}

TEST_F(TestRecalibrationStore, a_busy_device_is_written_10_ms_later)
{
    Started();
    saveStatus = services::NvmStatus::Busy;
    store.Store(LearnedInertia(2.0e-5f), Commissioned());
    EXPECT_EQ(0, store.Stores());

    saveStatus = services::NvmStatus::Ok;
    ForwardTime(state_machine::RecalibrationStore::busyRetry);

    EXPECT_EQ(2u, written.size());
    EXPECT_EQ(1, store.Stores());
}

TEST_F(TestRecalibrationStore, a_failed_store_waits_for_the_next_window)
{
    Started();
    saveStatus = services::NvmStatus::WriteFailed;
    store.Store(LearnedInertia(2.0e-5f), Commissioned());
    saveStatus = services::NvmStatus::Ok;

    store.StoreIfDue();
    EXPECT_EQ(1u, written.size());

    ForwardTime(15min);
    store.StoreIfDue();

    EXPECT_EQ(2u, written.size());
    EXPECT_EQ(1, store.Stores());
}

TEST_F(TestRecalibrationStore, a_reset_stores_an_empty_model_at_once_and_restores_nothing_more)
{
    Started(services::LearnedModelData{});
    store.Store(LearnedInertia(2.0e-5f), Commissioned());
    std::optional<bool> stored;

    store.Reset([&stored](bool result)
        {
            stored = result;
        });

    ASSERT_EQ(2u, written.size());
    EXPECT_EQ(services::LearnedModelData{}, written.back());
    EXPECT_EQ(true, stored);
    EXPECT_EQ(2, store.Stores());
    EXPECT_EQ(state_machine::StoredModel{}, store.Restored(Commissioned()));
}

TEST_F(TestRecalibrationStore, a_reset_that_is_not_stored_reports_it)
{
    Started();
    saveStatus = services::NvmStatus::HardwareFault;
    std::optional<bool> stored;

    store.Reset([&stored](bool result)
        {
            stored = result;
        });

    EXPECT_EQ(false, stored);
}

TEST_F(TestRecalibrationStore, a_reset_during_a_store_is_written_once_the_store_completes)
{
    Load({});
    store.Start([]() {});
    infra::Function<void(services::NvmStatus)> storeDone;
    EXPECT_CALL(nvm, SaveLearnedModel(_, _)).WillOnce(SaveArg<1>(&storeDone));
    store.Store(LearnedInertia(2.0e-5f), Commissioned());
    std::optional<bool> stored;
    store.Reset([&stored](bool result)
        {
            stored = result;
        });
    EXPECT_FALSE(stored.has_value());

    ExpectStores();
    storeDone(services::NvmStatus::Ok);

    ASSERT_EQ(1u, written.size());
    EXPECT_EQ(services::LearnedModelData{}, written.back());
    EXPECT_EQ(true, stored);
}

TEST(TestCommissioningCheck, the_flux_linkage_of_the_commissioned_model_does_not_enter_the_check)
{
    auto drifted = Commissioned();
    drifted.fluxLinkage = foc::Weber{ 0.009f };

    EXPECT_EQ(state_machine::CommissioningCheck(Commissioned()), state_machine::CommissioningCheck(drifted));
}
