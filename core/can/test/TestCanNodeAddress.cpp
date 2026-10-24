#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/CanNodeAddress.hpp"
#include <gtest/gtest.h>

namespace
{
    class TestCanNodeAddress
        : public testing::Test
    {};
}

TEST_F(TestCanNodeAddress, default_base_without_jumpers_is_node_one)
{
    EXPECT_EQ(1u, can::CanNodeId(can::defaultCanNodeIdBase, 0));
}

TEST_F(TestCanNodeAddress, board_id_is_added_to_the_configured_base)
{
    EXPECT_EQ(8u, can::CanNodeId(1, 7));
    EXPECT_EQ(13u, can::CanNodeId(10, 3));
}

TEST_F(TestCanNodeAddress, boards_jumpered_zero_to_seven_get_distinct_nodes_one_to_eight)
{
    for (uint8_t boardId = 0; boardId < 8; ++boardId)
        EXPECT_EQ(boardId + 1u, can::CanNodeId(can::defaultCanNodeIdBase, boardId));
}

TEST_F(TestCanNodeAddress, broadcast_base_falls_back_to_the_default_base)
{
    EXPECT_EQ(services::canBroadcastNodeId + 1u, can::defaultCanNodeIdBase);
    EXPECT_EQ(4u, can::CanNodeId(0, 3));
}

TEST_F(TestCanNodeAddress, highest_node_in_the_field_is_accepted)
{
    EXPECT_EQ(services::canIdNodeIdMask, can::CanNodeId(services::canIdNodeIdMask, 0));
    EXPECT_EQ(services::canIdNodeIdMask, can::CanNodeId(services::canIdNodeIdMask - 7, 7));
}

TEST_F(TestCanNodeAddress, base_that_overflows_the_node_field_falls_back_to_the_default_base)
{
    EXPECT_EQ(2u, can::CanNodeId(services::canIdNodeIdMask, 1));
    EXPECT_EQ(6u, can::CanNodeId(services::canIdNodeIdMask + 1, 5));
    EXPECT_EQ(1u, can::CanNodeId(UINT32_MAX, 0));
}
