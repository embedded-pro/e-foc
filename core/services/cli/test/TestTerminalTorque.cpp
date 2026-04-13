#include "core/foc/implementations/test_doubles/FocMock.hpp"
#include "core/services/cli/TerminalTorque.hpp"
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
    class TerminalTorqueTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        ::testing::StrictMock<foc::FocTorqueMock> focMock;
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
        services::TerminalFocTorqueInteractor terminalInteractor{ terminal, foc::Volts{ 12.0f }, focMock };

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

TEST_F(TerminalTorqueTest, set_torque)
{
    InvokeCommand("set_torque 2.5", [this]()
        {
            EXPECT_CALL(focMock, SetPoint(testing::_))
                .WillOnce(testing::Invoke([](const foc::IdAndIqPoint& point)
                    {
                        EXPECT_NEAR(point.first.Value(), 2.5f, 1e-5f);
                        EXPECT_NEAR(point.second.Value(), 0.0f, 1e-5f);
                    }));
        });

    ExecuteAllActions();
}

TEST_F(TerminalTorqueTest, set_torque_invalid_value)
{
    InvokeCommand("set_torque abc", [this]()
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
