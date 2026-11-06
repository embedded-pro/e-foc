#include "core/services/non_volatile_memory/FaultHistoryData.hpp"
#include <array>
#include <gtest/gtest.h>

namespace
{
    using Image = std::array<uint8_t, services::faultHistoryImageSize>;

    services::FaultRecord Record(uint8_t code, uint32_t uptimeMs)
    {
        return services::FaultRecord{ code, 1, 3, uptimeMs, 4012, -125 };
    }

    Image Serialized(const services::FaultHistoryData& history)
    {
        Image image{};
        services::SerializeFaultHistory(history, infra::MakeRange(image));
        return image;
    }
}

TEST(FaultHistoryImage, the_image_fills_the_128_byte_region_of_the_24LC04_with_room_to_spare)
{
    EXPECT_EQ(112u, services::faultHistoryImageSize);
}

TEST(FaultHistoryImage, a_history_reads_back_as_it_was_stored)
{
    services::FaultHistoryData history;
    history.powerUps = 7;
    history.records[0] = Record(2, 1000);
    history.records[5] = Record(10, 250);

    const auto image = Serialized(history);
    const auto read = services::DeserializeFaultHistory(infra::MakeConstByteRange(image));

    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(7, read->powerUps);
    EXPECT_EQ(history.records, read->records);
}

TEST(FaultHistoryImage, an_erased_region_is_not_a_fault_history)
{
    Image image{};
    image.fill(0xFF);

    EXPECT_FALSE(services::DeserializeFaultHistory(infra::MakeConstByteRange(image)).has_value());
}

TEST(FaultHistoryImage, another_layout_version_is_not_a_fault_history)
{
    auto image = Serialized(services::FaultHistoryData{});
    image[4] = services::FaultHistoryLayoutVersion + 1;

    EXPECT_FALSE(services::DeserializeFaultHistory(infra::MakeConstByteRange(image)).has_value());
}

TEST(FaultHistoryImage, a_damaged_record_reads_as_an_empty_slot_and_leaves_the_others)
{
    services::FaultHistoryData history;
    history.records[0] = Record(2, 1000);
    history.records[1] = Record(3, 2000);
    auto image = Serialized(history);

    image[services::faultHistoryHeaderSize + 3] ^= 0x01;
    const auto read = services::DeserializeFaultHistory(infra::MakeConstByteRange(image));

    ASSERT_TRUE(read.has_value());
    EXPECT_FALSE(read->records[0].has_value());
    EXPECT_EQ(history.records[1], read->records[1]);
}

TEST(FaultHistoryImage, a_damaged_power_up_count_reads_as_zero)
{
    services::FaultHistoryData history;
    history.powerUps = 9;
    auto image = Serialized(history);

    image[5] ^= 0x01;
    const auto read = services::DeserializeFaultHistory(infra::MakeConstByteRange(image));

    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(0, read->powerUps);
}

TEST(FaultHistoryImage, an_empty_slot_is_stored_erased)
{
    const auto image = Serialized(services::FaultHistoryData{});

    for (std::size_t index = services::faultHistoryHeaderSize; index != image.size(); ++index)
        EXPECT_EQ(0xFF, image[index]) << index;
}
