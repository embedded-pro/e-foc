#include "core/foc/interfaces/test_doubles/FocMock.hpp"
#include "core/services/cli/TerminalBase.hpp"
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
    struct TerminalBaseImpl
        : services::TerminalFocBaseInteractor
    {
        TerminalBaseImpl(services::TerminalWithStorage& terminal, foc::CurrentLoopTunable& controller)
            : services::TerminalFocBaseInteractor(terminal, controller)
        {}
    };

    class TerminalBaseTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        ::testing::StrictMock<foc::FocTorqueMock> controllerBaseMock;
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
        TerminalBaseImpl terminalInteractor{ terminal, controllerBaseMock };

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
    };
}

TEST_F(TerminalBaseTest, set_current_bandwidth)
{
    InvokeCommand("scbw 6283.2", [this]()
        {
            EXPECT_CALL(controllerBaseMock, SetCurrentTunings(testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalBaseTest, set_current_bandwidth_invalid_argument_count)
{
    InvokeCommand("set_current_bandwidth 6283.2 1.0", [this]()
        {
            ::testing::InSequence _;

            std::string header{ "ERROR: " };
            std::string payload{ "invalid number of arguments" };
            std::string newline{ "\r\n" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalBaseTest, set_current_bandwidth_invalid_value)
{
    InvokeCommand("set_current_bandwidth abc", [this]()
        {
            ::testing::InSequence _;

            std::string header{ "ERROR: " };
            std::string payload{ "invalid value. It should be a float." };
            std::string newline{ "\r\n" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}
