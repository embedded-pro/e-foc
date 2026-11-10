#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "tools/virtual_motor/platform/FileEeprom.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    class TestFileEeprom
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        ~TestFileEeprom() override
        {
            std::filesystem::remove(path);
        }

        infra::Function<void()> Done()
        {
            return [this]()
            {
                done.Call();
            };
        }

        std::string path{ (std::filesystem::temp_directory_path() / ("e-foc-eeprom-" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "-" + ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".nvm")).string() };
        testing::StrictMock<testing::MockFunction<void()>> done;
    };
}

TEST_F(TestFileEeprom, a_missing_file_reads_as_erased_memory)
{
    virtual_motor::FileEeprom eeprom{ path };
    std::array<uint8_t, 4> bytes{};

    EXPECT_CALL(done, Call());
    eeprom.ReadBuffer(infra::MakeByteRange(bytes), 0, Done());
    ExecuteAllActions();

    EXPECT_THAT(bytes, testing::Each(0xff));
    EXPECT_EQ(eeprom.Size(), virtual_motor::FileEeprom::size);
}

TEST_F(TestFileEeprom, a_write_survives_into_the_next_run)
{
    const std::array<uint8_t, 3> written{ 1, 2, 3 };
    {
        virtual_motor::FileEeprom eeprom{ path };
        EXPECT_CALL(done, Call());
        eeprom.WriteBuffer(infra::MakeConstByteRange(written), 100, Done());
        ExecuteAllActions();
    }

    virtual_motor::FileEeprom reopened{ path };
    std::array<uint8_t, 3> read{};
    EXPECT_CALL(done, Call());
    reopened.ReadBuffer(infra::MakeByteRange(read), 100, Done());
    ExecuteAllActions();

    EXPECT_EQ(read, written);
}

TEST_F(TestFileEeprom, completions_come_from_the_event_loop)
{
    virtual_motor::FileEeprom eeprom{ path };
    std::array<uint8_t, 1> bytes{};

    eeprom.ReadBuffer(infra::MakeByteRange(bytes), 0, Done());

    EXPECT_CALL(done, Call());
    ExecuteAllActions();
}

TEST_F(TestFileEeprom, erasing_clears_the_file_too)
{
    const std::array<uint8_t, 1> written{ 7 };
    {
        virtual_motor::FileEeprom eeprom{ path };
        EXPECT_CALL(done, Call()).Times(2);
        eeprom.WriteBuffer(infra::MakeConstByteRange(written), 0, Done());
        eeprom.Erase(Done());
        ExecuteAllActions();
    }

    virtual_motor::FileEeprom reopened{ path };
    std::array<uint8_t, 1> read{};
    EXPECT_CALL(done, Call());
    reopened.ReadBuffer(infra::MakeByteRange(read), 0, Done());
    ExecuteAllActions();

    EXPECT_EQ(read[0], 0xff);
}

TEST_F(TestFileEeprom, a_short_file_is_treated_as_erased)
{
    {
        std::ofstream file{ path, std::ios::binary };
        file.put(0x12);
    }

    virtual_motor::FileEeprom eeprom{ path };
    std::array<uint8_t, 1> read{};
    EXPECT_CALL(done, Call());
    eeprom.ReadBuffer(infra::MakeByteRange(read), 0, Done());
    ExecuteAllActions();

    EXPECT_EQ(read[0], 0xff);
}
