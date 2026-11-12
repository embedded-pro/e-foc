#include "core/services/fault_history/FaultHistory.hpp"
#include "core/services/non_volatile_memory/test_doubles/NonVolatileMemoryMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>
#include <vector>

namespace
{
    using namespace testing;

    constexpr uint8_t overvoltage{ 2 };
    constexpr uint8_t stall{ 8 };
    constexpr uint8_t speedMode{ 1 };

    class FaultHistoryTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Load(const services::FaultHistoryData& stored, services::NvmStatus status = services::NvmStatus::Ok)
        {
            EXPECT_CALL(nvm, LoadFaultHistory(_, _)).WillOnce(Invoke([this, stored, status](services::FaultHistoryData& history, const infra::Function<void(services::NvmStatus)>& onDone)
                {
                    EXPECT_TRUE(held);
                    history = stored;
                    onDone(status);
                }));
        }

        void ExpectStores()
        {
            EXPECT_CALL(nvm, SaveFaultHistory(_, _)).WillRepeatedly(Invoke([this](const services::FaultHistoryData& history, const infra::Function<void(services::NvmStatus)>& onDone)
                {
                    EXPECT_TRUE(held);
                    stores.push_back(history);
                    onDone(saveStatus);
                }));
        }

        void Started(const services::FaultHistoryData& stored = {})
        {
            Load(stored);
            ExpectStores();
            history.Start();
        }

        void Fault(uint8_t code, float busVoltage = 40.0f, float peakCurrent = 1.25f)
        {
            history.Record(code, services::FaultHistory::Conditions{ speedMode, foc::Volts{ busVoltage }, foc::Ampere{ peakCurrent } });
        }

        StrictMock<services::NonVolatileMemoryMock> nvm;
        bool held{ false };
        uint32_t holds{ 0 };
        services::NvmStatus saveStatus{ services::NvmStatus::Ok };
        std::vector<services::FaultHistoryData> stores;
        services::FaultHistory history{ nvm, [this]()
            {
                held = true;
                ++holds;
            },
            [this]()
            {
                held = false;
            } };
    };
}

TEST_F(FaultHistoryTest, a_power_up_counts_itself_and_stores_the_count)
{
    services::FaultHistoryData stored;
    stored.powerUps = 4;

    Started(stored);

    ASSERT_EQ(1u, stores.size());
    EXPECT_EQ(5, stores.back().powerUps);
    EXPECT_FALSE(held);
}

TEST_F(FaultHistoryTest, an_image_that_is_no_fault_history_starts_empty_at_power_up_1)
{
    Load(services::FaultHistoryData{}, services::NvmStatus::InvalidData);
    ExpectStores();

    history.Start();

    ASSERT_EQ(1u, stores.size());
    EXPECT_EQ(1, stores.back().powerUps);
    EXPECT_FALSE(history.OfAge(0).has_value());
}

TEST_F(FaultHistoryTest, the_power_up_count_saturates)
{
    services::FaultHistoryData stored;
    stored.powerUps = services::FaultHistory::maxPowerUps;

    Started(stored);

    EXPECT_EQ(services::FaultHistory::maxPowerUps, stores.back().powerUps);
}

TEST_F(FaultHistoryTest, a_load_refused_as_busy_is_retried_10_ms_later)
{
    EXPECT_CALL(nvm, LoadFaultHistory(_, _)).WillOnce(Invoke([](services::FaultHistoryData&, const infra::Function<void(services::NvmStatus)>& onDone)
        {
            onDone(services::NvmStatus::Busy);
        }));
    history.Start();
    testing::Mock::VerifyAndClearExpectations(&nvm);

    Load(services::FaultHistoryData{});
    ExpectStores();
    ForwardTime(services::FaultHistory::busyRetry);

    EXPECT_EQ(1u, stores.size());
}

TEST_F(FaultHistoryTest, a_request_made_after_the_load_runs_at_once)
{
    Started();
    uint32_t runs{ 0 };

    history.WhenLoaded([&runs]()
        {
            ++runs;
        });

    EXPECT_EQ(1u, runs);
}

TEST_F(FaultHistoryTest, a_request_made_before_the_load_completes_runs_once_the_stored_faults_are_known)
{
    infra::Function<void(services::NvmStatus)> loadDone;
    services::FaultHistoryData loaded;
    loaded.powerUps = 1;
    loaded.records[0] = services::FaultRecord{ overvoltage, speedMode, 1, 100, 4000, 125 };
    EXPECT_CALL(nvm, LoadFaultHistory(_, _)).WillOnce(Invoke([&loadDone, &loaded](services::FaultHistoryData& history, const infra::Function<void(services::NvmStatus)>& onDone)
        {
            history = loaded;
            loadDone = onDone;
        }));
    ExpectStores();
    history.Start();

    std::optional<uint8_t> seenCode;
    history.WhenLoaded([this, &seenCode]()
        {
            seenCode = history.OfAge(0)->code;
        });
    EXPECT_FALSE(seenCode.has_value());

    loadDone(services::NvmStatus::Ok);

    EXPECT_EQ(overvoltage, seenCode);
}

TEST_F(FaultHistoryTest, requests_beyond_the_waiting_capacity_release_the_oldest_at_once)
{
    infra::Function<void(services::NvmStatus)> loadDone;
    EXPECT_CALL(nvm, LoadFaultHistory(_, _)).WillOnce(Invoke([&loadDone](services::FaultHistoryData&, const infra::Function<void(services::NvmStatus)>& onDone)
        {
            loadDone = onDone;
        }));
    ExpectStores();
    history.Start();

    std::vector<std::size_t> order;
    for (std::size_t request = 0; request != services::FaultHistory::waitingCapacity + 1; ++request)
        history.WhenLoaded([&order, request]()
            {
                order.push_back(request);
            });
    ASSERT_EQ(1u, order.size());
    EXPECT_EQ(0u, order.front());

    loadDone(services::NvmStatus::Ok);

    ASSERT_EQ(services::FaultHistory::waitingCapacity + 1, order.size());
    EXPECT_EQ(services::FaultHistory::waitingCapacity, order.back());
}

TEST_F(FaultHistoryTest, a_fault_is_read_at_once_with_its_conditions)
{
    services::FaultHistoryData stored;
    stored.powerUps = 2;
    Started(stored);
    ForwardTime(std::chrono::milliseconds(1500));

    Fault(overvoltage, 41.234f, -2.5f);

    const auto record = history.OfAge(0);
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(overvoltage, record->code);
    EXPECT_EQ(speedMode, record->mode);
    EXPECT_EQ(3, record->powerUp);
    EXPECT_EQ(1500u, record->uptimeMs);
    EXPECT_EQ(4123, record->busVoltageCentivolts);
    EXPECT_EQ(-250, record->peakCurrentCentiamperes);
}

TEST_F(FaultHistoryTest, a_fault_is_stored_within_a_second_of_the_previous_store)
{
    Started();

    Fault(overvoltage);
    EXPECT_EQ(1u, stores.size());

    ForwardTime(services::FaultHistory::storeInterval);
    ASSERT_EQ(2u, stores.size());
    EXPECT_EQ(overvoltage, stores.back().records[0]->code);
}

TEST_F(FaultHistoryTest, faults_within_a_second_go_out_in_one_store)
{
    Started();
    ForwardTime(std::chrono::seconds(5));
    ASSERT_EQ(1u, stores.size());

    Fault(overvoltage);
    EXPECT_EQ(2u, stores.size());
    Fault(stall);
    ForwardTime(std::chrono::milliseconds(999));
    EXPECT_EQ(2u, stores.size());

    ForwardTime(std::chrono::milliseconds(1));
    ASSERT_EQ(3u, stores.size());
    EXPECT_EQ(stall, history.OfAge(0)->code);
    EXPECT_EQ(overvoltage, history.OfAge(1)->code);
}

TEST_F(FaultHistoryTest, a_store_refused_as_busy_is_retried_10_ms_later)
{
    Started();
    ForwardTime(std::chrono::seconds(5));
    saveStatus = services::NvmStatus::Busy;
    Fault(overvoltage);
    EXPECT_FALSE(held);

    saveStatus = services::NvmStatus::Ok;
    const auto before = stores.size();
    ForwardTime(services::FaultHistory::busyRetry);

    EXPECT_EQ(before + 1, stores.size());
}

TEST_F(FaultHistoryTest, a_store_that_does_not_read_back_is_retried_a_second_later)
{
    Started();
    ForwardTime(std::chrono::seconds(5));
    saveStatus = services::NvmStatus::HardwareFault;
    Fault(overvoltage);
    const auto failed = stores.size();

    saveStatus = services::NvmStatus::Ok;
    ForwardTime(services::FaultHistory::storeInterval - std::chrono::milliseconds(1));
    EXPECT_EQ(failed, stores.size());

    ForwardTime(std::chrono::milliseconds(1));
    ASSERT_EQ(failed + 1, stores.size());
    EXPECT_EQ(overvoltage, stores.back().records[0]->code);

    ForwardTime(std::chrono::seconds(5));
    EXPECT_EQ(failed + 1, stores.size());
}

TEST_F(FaultHistoryTest, the_oldest_of_eight_records_gives_way)
{
    Started();
    for (uint8_t fault = 1; fault != 10; ++fault)
    {
        ForwardTime(std::chrono::seconds(1));
        Fault(fault);
    }

    EXPECT_EQ(9, history.OfAge(0)->code);
    EXPECT_EQ(2, history.OfAge(7)->code);
    EXPECT_FALSE(history.OfAge(8).has_value());
}

TEST_F(FaultHistoryTest, records_of_an_earlier_power_up_are_older_whatever_their_uptime)
{
    services::FaultHistoryData stored;
    stored.powerUps = 1;
    stored.records[3] = services::FaultRecord{ stall, speedMode, 1, 900000, 0, 0 };
    Started(stored);
    ForwardTime(std::chrono::seconds(1));

    Fault(overvoltage);

    EXPECT_EQ(overvoltage, history.OfAge(0)->code);
    EXPECT_EQ(stall, history.OfAge(1)->code);
}

TEST_F(FaultHistoryTest, a_fault_before_the_history_is_loaded_is_kept)
{
    EXPECT_CALL(nvm, LoadFaultHistory(_, _)).WillOnce(Invoke([](services::FaultHistoryData&, const infra::Function<void(services::NvmStatus)>& onDone)
        {
            onDone(services::NvmStatus::Busy);
        }));
    history.Start();
    Fault(stall);
    testing::Mock::VerifyAndClearExpectations(&nvm);

    Load(services::FaultHistoryData{});
    ExpectStores();
    ForwardTime(services::FaultHistory::busyRetry);

    ASSERT_EQ(1u, stores.size());
    EXPECT_EQ(stall, stores.back().records[0]->code);
}

TEST_F(FaultHistoryTest, a_clear_empties_the_history_keeps_the_power_up_count_and_answers_once_stored)
{
    services::FaultHistoryData stored;
    stored.powerUps = 6;
    stored.records[0] = services::FaultRecord{ stall, speedMode, 6, 10, 0, 0 };
    Started(stored);
    std::optional<bool> cleared;

    history.Clear([&cleared](bool stored)
        {
            cleared = stored;
        });
    EXPECT_FALSE(cleared.has_value());
    ForwardTime(services::FaultHistory::storeInterval);

    EXPECT_EQ(true, cleared);
    EXPECT_FALSE(history.OfAge(0).has_value());
    EXPECT_FALSE(stores.back().records[0].has_value());
    EXPECT_EQ(7, stores.back().powerUps);
}

TEST_F(FaultHistoryTest, a_clear_whose_store_fails_answers_so)
{
    Started();
    saveStatus = services::NvmStatus::HardwareFault;
    std::optional<bool> cleared;

    history.Clear([&cleared](bool stored)
        {
            cleared = stored;
        });
    ForwardTime(services::FaultHistory::storeInterval);

    EXPECT_EQ(false, cleared);
}

TEST_F(FaultHistoryTest, every_load_and_store_is_held_as_outstanding_work)
{
    Started();
    ForwardTime(std::chrono::seconds(5));

    Fault(overvoltage);

    EXPECT_EQ(3u, holds);
    EXPECT_FALSE(held);
}
