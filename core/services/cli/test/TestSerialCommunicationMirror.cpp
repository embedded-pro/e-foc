#include "core/services/cli/SerialCommunicationMirror.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "gmock/gmock.h"

namespace
{
    using namespace testing;

    class SerialCommunicationMirrorTest
        : public Test
    {
    public:
        StrictMock<hal::SerialCommunicationCleanMock> primary;
        StrictMock<hal::SerialCommunicationCleanMock> secondary;
        services::SerialCommunicationMirror mirror{ primary, secondary };
    };

    TEST_F(SerialCommunicationMirrorTest, data_is_sent_on_both_links)
    {
        const std::array<uint8_t, 3> data{ 1, 2, 3 };

        EXPECT_CALL(secondary, SendData(_, _)).WillOnce(Invoke([&](infra::ConstByteRange sent, infra::Function<void()>)
            {
                EXPECT_EQ(sent.size(), data.size());
            }));
        EXPECT_CALL(primary, SendData(_, _)).WillOnce(Invoke([&](infra::ConstByteRange sent, infra::Function<void()>)
            {
                EXPECT_EQ(sent.size(), data.size());
            }));

        mirror.SendData(infra::MakeByteRange(data), infra::emptyFunction);
    }

    TEST_F(SerialCommunicationMirrorTest, completion_follows_the_primary_link_only)
    {
        const std::array<uint8_t, 1> data{ 7 };
        int completed = 0;
        infra::Function<void()> primaryDone;

        EXPECT_CALL(secondary, SendData(_, _)).WillOnce(Invoke([&](infra::ConstByteRange, infra::Function<void()> done)
            {
                (void)done;
            }));
        EXPECT_CALL(primary, SendData(_, _)).WillOnce(Invoke([&](infra::ConstByteRange, infra::Function<void()> done)
            {
                primaryDone = done;
            }));

        mirror.SendData(infra::MakeByteRange(data), [&]()
            {
                ++completed;
            });

        EXPECT_EQ(completed, 0);
        primaryDone();
        EXPECT_EQ(completed, 1);
    }

    TEST_F(SerialCommunicationMirrorTest, input_from_either_link_reaches_the_receiver)
    {
        infra::Function<void(infra::ConstByteRange)> primaryInput;
        infra::Function<void(infra::ConstByteRange)> secondaryInput;
        std::size_t received = 0;

        EXPECT_CALL(primary, ReceiveData(_)).WillOnce(Invoke([&](infra::Function<void(infra::ConstByteRange)> callback)
            {
                primaryInput = callback;
            }));
        EXPECT_CALL(secondary, ReceiveData(_)).WillOnce(Invoke([&](infra::Function<void(infra::ConstByteRange)> callback)
            {
                secondaryInput = callback;
            }));

        mirror.ReceiveData([&](infra::ConstByteRange data)
            {
                received += data.size();
            });

        const std::array<uint8_t, 2> fromPrimary{ 1, 2 };
        const std::array<uint8_t, 3> fromSecondary{ 3, 4, 5 };
        primaryInput(infra::MakeConstByteRange(fromPrimary));
        secondaryInput(infra::MakeConstByteRange(fromSecondary));

        EXPECT_EQ(received, 5u);
    }

    TEST_F(SerialCommunicationMirrorTest, clearing_the_receiver_clears_both_links)
    {
        EXPECT_CALL(primary, ReceiveData(_)).WillOnce(Invoke([](infra::Function<void(infra::ConstByteRange)> callback)
            {
                EXPECT_FALSE(static_cast<bool>(callback));
            }));
        EXPECT_CALL(secondary, ReceiveData(_)).WillOnce(Invoke([](infra::Function<void(infra::ConstByteRange)> callback)
            {
                EXPECT_FALSE(static_cast<bool>(callback));
            }));

        mirror.ReceiveData(nullptr);
    }
}
