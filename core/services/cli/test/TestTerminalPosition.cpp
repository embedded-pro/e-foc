#include "core/foc/implementations/test_doubles/FocMock.hpp"
#include "core/services/cli/TerminalPosition.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/stream/test/StreamMock.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/test_helper/MockHelpers.hpp"
#include "services/util/Terminal.hpp"
#include "gmock/gmock.h"
#include <optional>

namespace
{
    class TerminalPositionTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        ::testing::StrictMock<foc::FocPositionMock> focMock;
        ::testing::StrictMock<infra::StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriterMock };
        services::TracerToStream tracer{ stream };
        ::testing::StrictMock<hal::SerialCommunicationMock> communication;
        infra::Execute execute{ [this]()
            {
                EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { '>', ' ' } }), testing::_));
            } };
        services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<128, 5> terminalWithCommands{ communication, tracer };
        services::TerminalWithStorage::WithMaxSize<10> terminal{ terminalWithCommands, tracer };
        services::TerminalFocPositionInteractor terminalInteractor{ terminal, foc::Volts{ 12.0f }, focMock };

        void InvokeCommand(std::string command, const std::function<void()>& onCommandReceived)
        {
            ::testing::InSequence _;

            for (const auto& data : command)
                EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ static_cast<uint8_t>(data) }), testing::_));

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { '\r', '\n' } }), testing::_));
            onCommandReceived();
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { '>', ' ' } }), testing::_));

            communication.dataReceived(infra::MakeStringByteRange(command + "\r"));
        }

        void ExpectError(const std::string& message)
        {
            ::testing::InSequence _;

            std::string header{ "ERROR: " };
            std::string newline{ "\r\n" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(message.begin(), message.end())), testing::_));
        }
    };
}

TEST_F(TerminalPositionTest, set_speed_pid)
{
    InvokeCommand("sspid 0.5 0.1 0.01", [this]()
        {
            EXPECT_CALL(focMock, SetSpeedTunings(testing::_, testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalPositionTest, set_speed_pid_invalid_argument_count)
{
    InvokeCommand("set_speed_pid 0.5 0.1", [this]()
        {
            ExpectError("invalid number of arguments");
        });

    ExecuteAllActions();
}

TEST_F(TerminalPositionTest, set_speed_pid_invalid_kp)
{
    InvokeCommand("set_speed_pid abc 0.1 0.01", [this]()
        {
            ExpectError("invalid value. It should be a float.");
        });

    ExecuteAllActions();
}

TEST_F(TerminalPositionTest, set_speed_pid_invalid_ki)
{
    InvokeCommand("set_speed_pid 0.5 abc 0.01", [this]()
        {
            ExpectError("invalid value. It should be a float.");
        });

    ExecuteAllActions();
}

TEST_F(TerminalPositionTest, set_speed_pid_invalid_kd)
{
    InvokeCommand("set_speed_pid 0.5 0.1 abc", [this]()
        {
            ExpectError("invalid value. It should be a float.");
        });

    ExecuteAllActions();
}

TEST_F(TerminalPositionTest, set_position_pid)
{
    InvokeCommand("sppid 5.0 0.1 0.0", [this]()
        {
            EXPECT_CALL(focMock, SetPositionTunings(testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalPositionTest, set_position_pid_invalid_argument_count)
{
    InvokeCommand("set_position_pid 5.0", [this]()
        {
            ExpectError("invalid number of arguments");
        });

    ExecuteAllActions();
}

TEST_F(TerminalPositionTest, set_position_pid_invalid_kp)
{
    InvokeCommand("set_position_pid abc 0.1 0.0", [this]()
        {
            ExpectError("invalid value. It should be a float.");
        });

    ExecuteAllActions();
}

TEST_F(TerminalPositionTest, set_position_pid_invalid_ki)
{
    InvokeCommand("set_position_pid 5.0 abc 0.0", [this]()
        {
            ExpectError("invalid value. It should be a float.");
        });

    ExecuteAllActions();
}

TEST_F(TerminalPositionTest, set_position_pid_invalid_kd)
{
    InvokeCommand("set_position_pid 5.0 0.1 abc", [this]()
        {
            ExpectError("invalid value. It should be a float.");
        });

    ExecuteAllActions();
}

TEST_F(TerminalPositionTest, set_position)
{
    InvokeCommand("sp 3.14", [this]()
        {
            EXPECT_CALL(focMock, SetPoint(testing::_))
                .WillOnce(testing::Invoke([](foc::Radians r)
                    {
                        EXPECT_NEAR(r.Value(), 3.14f, 1e-5f);
                    }));
        });

    ExecuteAllActions();
}

TEST_F(TerminalPositionTest, set_position_invalid_argument_count)
{
    InvokeCommand("set_position 2.5 3.5", [this]()
        {
            ExpectError("invalid number of arguments.");
        });

    ExecuteAllActions();
}

TEST_F(TerminalPositionTest, set_position_invalid_value)
{
    InvokeCommand("set_position abc", [this]()
        {
            ExpectError("invalid value. It should be a float.");
        });

    ExecuteAllActions();
}
