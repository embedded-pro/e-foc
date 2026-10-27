#include "core/services/non_volatile_memory/NvmEepromRegion.hpp"
#include "hal/interfaces/test_doubles/EepromStub.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include <gtest/gtest.h>

namespace
{
    // EepromStub with 1024 bytes; tests use a 128-byte region starting at baseAddress 256.
    class NvmEepromRegionTest
        : public ::testing::Test
        , public infra::EventDispatcherFixture
    {
    protected:
        hal::EepromStub eeprom{ 1024 };
        services::NvmEepromRegion region{ eeprom, 256, 128 };

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

TEST_F(NvmEepromRegionTest, write_stores_data_at_correct_base_address)
{
    const std::array<uint8_t, 3> data = { 0x11, 0x22, 0x33 };
    bool writeDone = false;

    region.Write(infra::MakeConstByteRange(data), [&]
        {
            writeDone = true;
        });
    RunUntilDone(writeDone);

    std::array<uint8_t, 3> readback{};
    bool readDone = false;
    eeprom.ReadBuffer(infra::MakeByteRange(readback), 256, [&]
        {
            readDone = true;
        });
    RunUntilDone(readDone);

    EXPECT_EQ(readback[0], 0x11);
    EXPECT_EQ(readback[1], 0x22);
    EXPECT_EQ(readback[2], 0x33);
}

TEST_F(NvmEepromRegionTest, write_does_not_affect_adjacent_region)
{
    services::NvmEepromRegion adjacent{ eeprom, 384, 128 };
    const std::array<uint8_t, 3> data = { 0x77, 0x88, 0x99 };
    bool writeDone = false;

    region.Write(infra::MakeConstByteRange(data), [&]
        {
            writeDone = true;
        });
    RunUntilDone(writeDone);

    std::array<uint8_t, 3> adjacentData{};
    bool readDone = false;
    adjacent.Read(infra::MakeByteRange(adjacentData), [&]
        {
            readDone = true;
        });
    RunUntilDone(readDone);

    EXPECT_EQ(adjacentData[0], 0xFF);
    EXPECT_EQ(adjacentData[1], 0xFF);
    EXPECT_EQ(adjacentData[2], 0xFF);
}

TEST_F(NvmEepromRegionTest, read_retrieves_data_at_base_address)
{
    const std::array<uint8_t, 2> seed = { 0xAB, 0xCD };
    bool seedDone = false;
    eeprom.WriteBuffer(infra::MakeConstByteRange(seed), 256, [&]
        {
            seedDone = true;
        });
    RunUntilDone(seedDone);

    std::array<uint8_t, 2> readback{};
    bool readDone = false;
    region.Read(infra::MakeByteRange(readback), [&]
        {
            readDone = true;
        });
    RunUntilDone(readDone);

    EXPECT_EQ(readback[0], 0xAB);
    EXPECT_EQ(readback[1], 0xCD);
}

TEST_F(NvmEepromRegionTest, erase_fills_region_with_0xFF)
{
    const std::array<uint8_t, 5> data = { 0x00, 0x11, 0x22, 0x33, 0x44 };
    bool writeDone = false;
    region.Write(infra::MakeConstByteRange(data), [&]
        {
            writeDone = true;
        });
    RunUntilDone(writeDone);

    bool eraseDone = false;
    region.Erase([&]
        {
            eraseDone = true;
        });
    RunUntilDone(eraseDone);

    // The whole region, not just the bytes written above
    std::array<uint8_t, 128> readback{};
    bool readDone = false;
    region.Read(infra::MakeByteRange(readback), [&]
        {
            readDone = true;
        });
    RunUntilDone(readDone);

    for (auto byte : readback)
        EXPECT_EQ(byte, 0xFF);
}

TEST_F(NvmEepromRegionTest, erase_does_not_affect_region_after_it)
{
    services::NvmEepromRegion after{ eeprom, 384, 128 };
    const std::array<uint8_t, 3> data = { 0x88, 0x99, 0xAA };
    bool writeDone = false;
    after.Write(infra::MakeConstByteRange(data), [&]
        {
            writeDone = true;
        });
    RunUntilDone(writeDone);

    bool eraseDone = false;
    region.Erase([&]
        {
            eraseDone = true;
        });
    RunUntilDone(eraseDone);

    // Fails if Erase writes the whole erasePattern
    std::array<uint8_t, 3> readback{};
    bool readDone = false;
    after.Read(infra::MakeByteRange(readback), [&]
        {
            readDone = true;
        });
    RunUntilDone(readDone);

    EXPECT_EQ(readback[0], 0x88);
    EXPECT_EQ(readback[1], 0x99);
    EXPECT_EQ(readback[2], 0xAA);
}

TEST_F(NvmEepromRegionTest, erase_does_not_affect_region_before_it)
{
    services::NvmEepromRegion before{ eeprom, 128, 128 };
    const std::array<uint8_t, 3> data = { 0x55, 0x66, 0x77 };
    bool writeDone = false;
    before.Write(infra::MakeConstByteRange(data), [&]
        {
            writeDone = true;
        });
    RunUntilDone(writeDone);

    bool eraseDone = false;
    region.Erase([&]
        {
            eraseDone = true;
        });
    RunUntilDone(eraseDone);

    std::array<uint8_t, 3> readback{};
    bool readDone = false;
    before.Read(infra::MakeByteRange(readback), [&]
        {
            readDone = true;
        });
    RunUntilDone(readDone);

    EXPECT_EQ(readback[0], 0x55);
    EXPECT_EQ(readback[1], 0x66);
    EXPECT_EQ(readback[2], 0x77);
}

TEST_F(NvmEepromRegionTest, size_returns_configured_region_size)
{
    EXPECT_EQ(region.Size(), 128u);
}
