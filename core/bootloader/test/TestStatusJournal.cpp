#include "core/bootloader/StatusJournal.hpp"
#include "core/bootloader/test/FlashFake.hpp"
#include <gmock/gmock.h>

namespace
{
    using application::UpgradePackStatus;

    class TestStatusJournal
        : public ::testing::Test
    {
    public:
        bootloader::FlashFake flash{ 1, 4 * bootloader::FlashFake::programmingUnit };
        bootloader::StatusJournal journal{ flash };
    };
}

TEST_F(TestStatusJournal, an_erased_journal_reads_empty_without_an_error)
{
    EXPECT_EQ(UpgradePackStatus::empty, journal.Status());
    EXPECT_EQ(0u, journal.ErrorCode());
}

TEST_F(TestStatusJournal, a_recorded_status_reads_back)
{
    journal.Record(UpgradePackStatus::readyToDeploy, 0);

    EXPECT_EQ(UpgradePackStatus::readyToDeploy, journal.Status());
}

TEST_F(TestStatusJournal, the_latest_record_wins)
{
    journal.Record(UpgradePackStatus::downloaded, 0);
    journal.Record(UpgradePackStatus::readyToDeploy, 0);
    journal.Record(UpgradePackStatus::deployStarted, 0);

    EXPECT_EQ(UpgradePackStatus::deployStarted, journal.Status());
}

TEST_F(TestStatusJournal, an_error_code_reads_back_with_its_status)
{
    journal.Record(UpgradePackStatus::invalid, 1000);

    EXPECT_EQ(UpgradePackStatus::invalid, journal.Status());
    EXPECT_EQ(1000u, journal.ErrorCode());
}

TEST_F(TestStatusJournal, each_record_takes_a_fresh_programming_unit)
{
    journal.Record(UpgradePackStatus::downloaded, 0);
    journal.Record(UpgradePackStatus::readyToDeploy, 0);

    EXPECT_TRUE(flash.programmed[0]);
    EXPECT_TRUE(flash.programmed[1]);
    EXPECT_FALSE(flash.programmed[2]);
}

TEST_F(TestStatusJournal, recording_the_current_status_again_writes_nothing)
{
    journal.Record(UpgradePackStatus::deployStarted, 0);

    journal.Record(UpgradePackStatus::deployStarted, 0);
    journal.Record(UpgradePackStatus::deployStarted, 0);

    EXPECT_TRUE(flash.programmed[0]);
    EXPECT_FALSE(flash.programmed[1]);
}

TEST_F(TestStatusJournal, the_same_status_with_another_error_is_written)
{
    journal.Record(UpgradePackStatus::invalid, 0);

    journal.Record(UpgradePackStatus::invalid, 1000);

    EXPECT_TRUE(flash.programmed[1]);
    EXPECT_EQ(1000u, journal.ErrorCode());
}

TEST_F(TestStatusJournal, clearing_returns_to_empty)
{
    journal.Record(UpgradePackStatus::readyToDeploy, 0);

    journal.Clear();

    EXPECT_EQ(UpgradePackStatus::empty, journal.Status());
}

TEST_F(TestStatusJournal, a_torn_entry_is_ignored_and_skipped)
{
    journal.Record(UpgradePackStatus::downloaded, 0);
    flash.Load({ 0x45, 0x46, 0x4a, 0x31, 0xfc }, bootloader::FlashFake::programmingUnit);

    EXPECT_EQ(UpgradePackStatus::downloaded, journal.Status());

    journal.Record(UpgradePackStatus::readyToDeploy, 0);

    EXPECT_EQ(UpgradePackStatus::readyToDeploy, journal.Status());
    EXPECT_TRUE(flash.programmed[2]);
}

TEST_F(TestStatusJournal, a_full_journal_is_erased_and_restarted_with_the_new_status)
{
    for (auto i = 0; i != 4; ++i)
        journal.Record(i % 2 == 0 ? UpgradePackStatus::downloaded : UpgradePackStatus::invalid, 0);

    journal.Record(UpgradePackStatus::readyToDeploy, 0);

    EXPECT_EQ(1u, flash.erasures);
    EXPECT_EQ(UpgradePackStatus::readyToDeploy, journal.Status());
    EXPECT_TRUE(flash.programmed[0]);
    EXPECT_FALSE(flash.programmed[1]);
}

TEST_F(TestStatusJournal, a_journal_over_several_sectors_uses_all_of_them)
{
    bootloader::FlashFake large{ 2, 2 * bootloader::FlashFake::programmingUnit };
    bootloader::StatusJournal spread{ large };

    for (auto i = 0; i != 3; ++i)
        spread.Record(i % 2 == 0 ? UpgradePackStatus::downloaded : UpgradePackStatus::invalid, 0);
    spread.Record(UpgradePackStatus::deployed, 0);

    EXPECT_EQ(0u, large.erasures);
    EXPECT_EQ(UpgradePackStatus::deployed, spread.Status());
}
