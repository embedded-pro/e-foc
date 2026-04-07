#include "hal/interfaces/test_doubles/FlashStub.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "source/services/NonVolatileMemory/NvmFlashRegion.hpp"
#include <gmock/gmock.h>

namespace
{
    using namespace testing;

    // FlashStub with 4 sectors of 128 bytes each; tests use sector 2.
    class NvmFlashRegionTest
        : public ::testing::Test
        , public infra::EventDispatcherFixture
    {
    protected:
        hal::FlashStub flash{ 4, 128 };
        services::NvmFlashRegion region{ flash, 2 };

        void RunUntilDone(bool& done)
        {
            constexpr int maxIterations = 1000;
            for (int i = 0; i < maxIterations; ++i)
            {
                if (done)
                    return;
                ExecuteAllActions();
            }
            FAIL() << "Async callback was not invoked within " << maxIterations << " iterations";
        }
    };
}

TEST_F(NvmFlashRegionTest, write_stores_data_in_the_assigned_sector)
{
    const std::array<uint8_t, 3> data = { 0xAA, 0xBB, 0xCC };
    bool done = false;

    region.Write(infra::MakeConstByteRange(data), [&]
        {
            done = true;
        });
    RunUntilDone(done);

    // FlashStub WriteBuffer ANDs bytes; sector[2] starts as 0xFF so 0xFF & 0xAA == 0xAA
    EXPECT_EQ(flash.sectors[2][0], 0xAA);
    EXPECT_EQ(flash.sectors[2][1], 0xBB);
    EXPECT_EQ(flash.sectors[2][2], 0xCC);
}

TEST_F(NvmFlashRegionTest, write_does_not_touch_other_sectors)
{
    const std::array<uint8_t, 1> data = { 0x00 };
    bool done = false;

    region.Write(infra::MakeConstByteRange(data), [&]
        {
            done = true;
        });
    RunUntilDone(done);

    for (auto byte : flash.sectors[0])
        EXPECT_EQ(byte, 0xFF);
    for (auto byte : flash.sectors[1])
        EXPECT_EQ(byte, 0xFF);
    for (auto byte : flash.sectors[3])
        EXPECT_EQ(byte, 0xFF);
}

TEST_F(NvmFlashRegionTest, read_retrieves_data_from_the_assigned_sector)
{
    flash.sectors[2][0] = 0x42;
    flash.sectors[2][1] = 0x43;

    std::array<uint8_t, 2> buf{};
    bool done = false;

    region.Read(infra::MakeByteRange(buf), [&]
        {
            done = true;
        });
    RunUntilDone(done);

    EXPECT_EQ(buf[0], 0x42);
    EXPECT_EQ(buf[1], 0x43);
}

TEST_F(NvmFlashRegionTest, erase_resets_only_the_assigned_sector_to_0xFF)
{
    flash.sectors[2] = std::vector<uint8_t>(128, 0x00);
    bool done = false;

    region.Erase([&]
        {
            done = true;
        });
    RunUntilDone(done);

    for (auto byte : flash.sectors[2])
        EXPECT_EQ(byte, 0xFF);

    // Other sectors untouched (FlashStub initialises all sectors to 0xFF)
    for (auto byte : flash.sectors[0])
        EXPECT_EQ(byte, 0xFF);
    for (auto byte : flash.sectors[1])
        EXPECT_EQ(byte, 0xFF);
    for (auto byte : flash.sectors[3])
        EXPECT_EQ(byte, 0xFF);
}

TEST_F(NvmFlashRegionTest, size_returns_the_sector_size)
{
    EXPECT_EQ(region.Size(), 128u);
}
