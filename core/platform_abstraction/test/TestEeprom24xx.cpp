#include "core/platform_abstraction/Eeprom24xx.hpp"
#include "hal/interfaces/test_doubles/I2cMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace
{
    using testing::ElementsAreArray;

    class TestEeprom24xx
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        infra::Function<void()> Done()
        {
            return [this]()
            {
                onDone.callback();
            };
        }

        void SendCompletes(hal::Result result = hal::Result::complete)
        {
            auto onSent = i2c.onSent;
            onSent(result, 0);
        }

        void ReceiveCompletes(hal::Result result = hal::Result::complete)
        {
            auto onReceived = i2c.onReceived;
            onReceived(result);
        }

        static std::vector<uint8_t> Frame(uint8_t wordAddress, std::vector<uint8_t> data)
        {
            data.insert(data.begin(), wordAddress);
            return data;
        }

        const hal::I2cAddress block0{ 0x50 };
        const hal::I2cAddress block1{ 0x51 };
        testing::StrictMock<hal::I2cMasterMockWithoutAutomaticDone> i2c;
        testing::StrictMock<infra::MockCallback<void()>> onDone;
        application::Eeprom24xx eeprom{ i2c, application::Eeprom24xx::Config{} };
    };
}

TEST_F(TestEeprom24xx, reports_the_configured_size)
{
    EXPECT_EQ(eeprom.Size(), 512u);
}

TEST_F(TestEeprom24xx, reads_by_sending_the_word_address_then_receiving)
{
    std::array<uint8_t, 4> buffer{};

    EXPECT_CALL(i2c, SendDataMock(block0, hal::Action::repeatedStart, std::vector<uint8_t>{ 0x10 }));
    eeprom.ReadBuffer(infra::MakeRange(buffer), 0x10, Done());

    EXPECT_CALL(i2c, ReceiveDataMock(block0, hal::Action::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4 }));
    SendCompletes();

    EXPECT_CALL(onDone, callback());
    ReceiveCompletes();

    EXPECT_THAT(buffer, ElementsAreArray({ 1, 2, 3, 4 }));
}

TEST_F(TestEeprom24xx, a_read_across_a_block_boundary_selects_the_next_block_in_the_device_address)
{
    std::array<uint8_t, 4> buffer{};

    EXPECT_CALL(i2c, SendDataMock(block0, hal::Action::repeatedStart, std::vector<uint8_t>{ 0xfe }));
    eeprom.ReadBuffer(infra::MakeRange(buffer), 0xfe, Done());

    EXPECT_CALL(i2c, ReceiveDataMock(block0, hal::Action::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2 }));
    SendCompletes();

    EXPECT_CALL(i2c, SendDataMock(block1, hal::Action::repeatedStart, std::vector<uint8_t>{ 0x00 }));
    ReceiveCompletes();

    EXPECT_CALL(i2c, ReceiveDataMock(block1, hal::Action::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 3, 4 }));
    SendCompletes();

    EXPECT_CALL(onDone, callback());
    ReceiveCompletes();

    EXPECT_THAT(buffer, ElementsAreArray({ 1, 2, 3, 4 }));
}

TEST_F(TestEeprom24xx, an_unacknowledged_read_completes_with_erased_bytes)
{
    std::array<uint8_t, 3> buffer{ 1, 2, 3 };

    EXPECT_CALL(i2c, SendDataMock(block0, hal::Action::repeatedStart, std::vector<uint8_t>{ 0x00 }));
    eeprom.ReadBuffer(infra::MakeRange(buffer), 0, Done());

    EXPECT_CALL(onDone, callback());
    SendCompletes(hal::Result::partialComplete);

    EXPECT_THAT(buffer, ElementsAreArray({ 0xff, 0xff, 0xff }));
}

TEST_F(TestEeprom24xx, a_write_is_split_at_page_boundaries_and_waits_out_each_write_cycle)
{
    std::vector<uint8_t> data(20);
    for (std::size_t i = 0; i != data.size(); ++i)
        data[i] = static_cast<uint8_t>(i);

    EXPECT_CALL(i2c, SendDataMock(block0, hal::Action::stop, Frame(0x0c, { 0, 1, 2, 3 })));
    eeprom.WriteBuffer(infra::MakeRange(data), 0x0c, Done());

    SendCompletes();
    ForwardTime(std::chrono::milliseconds(4));

    EXPECT_CALL(i2c, SendDataMock(block0, hal::Action::stop, Frame(0x10, { 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 })));
    ForwardTime(std::chrono::milliseconds(1));

    SendCompletes();

    EXPECT_CALL(onDone, callback());
    ForwardTime(std::chrono::milliseconds(5));
}

TEST_F(TestEeprom24xx, a_write_to_the_second_block_selects_it_in_the_device_address)
{
    std::vector<uint8_t> data{ 0xaa, 0xbb };

    EXPECT_CALL(i2c, SendDataMock(block1, hal::Action::stop, Frame(0x02, { 0xaa, 0xbb })));
    eeprom.WriteBuffer(infra::MakeRange(data), 0x102, Done());

    SendCompletes();

    EXPECT_CALL(onDone, callback());
    ForwardTime(std::chrono::milliseconds(5));
}

TEST_F(TestEeprom24xx, an_unacknowledged_page_moves_on_without_a_write_cycle)
{
    std::vector<uint8_t> data(18, 0x5a);

    EXPECT_CALL(i2c, SendDataMock(block0, hal::Action::stop, Frame(0x00, std::vector<uint8_t>(16, 0x5a))));
    eeprom.WriteBuffer(infra::MakeRange(data), 0, Done());

    EXPECT_CALL(i2c, SendDataMock(block0, hal::Action::stop, Frame(0x10, { 0x5a, 0x5a })));
    SendCompletes(hal::Result::partialComplete);

    EXPECT_CALL(onDone, callback());
    SendCompletes(hal::Result::partialComplete);
}

TEST_F(TestEeprom24xx, erase_writes_the_erased_pattern_over_every_page)
{
    application::Eeprom24xx small{ i2c, application::Eeprom24xx::Config{ 0x50, 32, 16, std::chrono::milliseconds(5) } };

    EXPECT_CALL(i2c, SendDataMock(block0, hal::Action::stop, Frame(0x00, std::vector<uint8_t>(16, 0xff))));
    small.Erase(Done());

    SendCompletes();

    EXPECT_CALL(i2c, SendDataMock(block0, hal::Action::stop, Frame(0x10, std::vector<uint8_t>(16, 0xff))));
    ForwardTime(std::chrono::milliseconds(5));

    SendCompletes();

    EXPECT_CALL(onDone, callback());
    ForwardTime(std::chrono::milliseconds(5));
}
