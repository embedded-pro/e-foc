#include "tools/can_commander/logic/FirmwareRefusal.hpp"
#include "gtest/gtest.h"

using services::FwuError;
using tool::FirmwareRefusalText;
using tool::FirmwareStep;

TEST(FirmwareRefusalTest, not_ready_on_a_download_means_the_pack_is_not_for_this_board)
{
    EXPECT_EQ("the pack is not for this board and microcontroller, and nothing was staged.", FirmwareRefusalText(FwuError::notReady, FirmwareStep::download));
}

TEST(FirmwareRefusalTest, not_ready_on_an_apply_means_nothing_is_staged)
{
    EXPECT_EQ("no pack is staged on it; download one first.", FirmwareRefusalText(FwuError::notReady, FirmwareStep::apply));
}

TEST(FirmwareRefusalTest, invalid_state_means_the_controller_ended_the_download)
{
    const auto text = FirmwareRefusalText(FwuError::invalidState, FirmwareStep::download);
    EXPECT_EQ("it was no longer receiving the pack: an emergency stop, an abort or 30 s without a block ended the download on the controller, and nothing was staged.", text);
}

TEST(FirmwareRefusalTest, session_timeout_means_the_controller_waited_too_long)
{
    EXPECT_EQ("it ended the download after 30 s without a block, and nothing was staged.", FirmwareRefusalText(FwuError::sessionTimeout, FirmwareStep::download));
}

TEST(FirmwareRefusalTest, busy_reads_the_same_for_a_download_and_an_apply)
{
    const auto download = FirmwareRefusalText(FwuError::busy, FirmwareStep::download);
    EXPECT_EQ("the motor was not stopped or had work outstanding. Stop it, wait, and try again.", download);
    EXPECT_EQ(download, FirmwareRefusalText(FwuError::busy, FirmwareStep::apply));
}

TEST(FirmwareRefusalTest, every_other_answer_has_its_own_text)
{
    EXPECT_EQ("the pack did not arrive intact, and nothing was staged.", FirmwareRefusalText(FwuError::crcMismatch, FirmwareStep::download));
    EXPECT_EQ("the pack does not fit its staging slot.", FirmwareRefusalText(FwuError::invalidSize, FirmwareStep::download));
    EXPECT_EQ("it could not write its staging slot.", FirmwareRefusalText(FwuError::writeError, FirmwareStep::download));
    EXPECT_EQ("a block arrived out of order.", FirmwareRefusalText(FwuError::sequenceError, FirmwareStep::download));
}

TEST(FirmwareRefusalTest, an_unknown_answer_shows_its_code)
{
    EXPECT_EQ("it answered with code 42.", FirmwareRefusalText(static_cast<FwuError>(42), FirmwareStep::download));
}
