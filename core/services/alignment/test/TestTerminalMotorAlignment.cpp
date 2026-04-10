#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/test_helper/MockHelpers.hpp"
#include "services/util/Terminal.hpp"
#include "core/services/alignment/MotorAlignment.hpp"
#include "core/services/alignment/TerminalMotorAlignment.hpp"
#include "gmock/gmock.h"

namespace
{
    class StreamWriterMock
        : public infra::StreamWriter
    {
    public:
        using StreamWriter::StreamWriter;

        MOCK_METHOD2(Insert, void(infra::ConstByteRange range, infra::StreamErrorPolicy& errorPolicy));
        MOCK_CONST_METHOD0(Available, std::size_t());
        MOCK_CONST_METHOD0(ConstructSaveMarker, std::size_t());
        MOCK_CONST_METHOD1(GetProcessedBytesSince, std::size_t(std::size_t marker));
        MOCK_METHOD1(SaveState, infra::ByteRange(std::size_t marker));
        MOCK_METHOD1(RestoreState, void(infra::ByteRange range));
        MOCK_METHOD1(Overwrite, infra::ByteRange(std::size_t marker));
    };

    class MotorAlignmentMock
        : public services::MotorAlignment
    {
    public:
        MOCK_METHOD3(ForceAlignment, void(std::size_t polePairs, const AlignmentConfig& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone));
    };

    class TerminalMotorAlignmentTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        ::testing::StrictMock<MotorAlignmentMock> alignmentMock;
        ::testing::StrictMock<StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriterMock };
        services::TracerToStream tracer{ stream };
        ::testing::StrictMock<hal::SerialCommunicationMock> communication;
        infra::Execute execute{ [this]()
            {
                EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { '>', ' ' } }), testing::_));
            } };
        services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<128, 5> terminalWithCommands{ communication, tracer };
        services::TerminalWithStorage::WithMaxSize<10> terminal{ terminalWithCommands, tracer };
        services::TerminalMotorAlignment terminalAlignment{ terminal, tracer, alignmentMock };

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

TEST_F(TerminalMotorAlignmentTest, force_alignment)
{
    InvokeCommand("force_alignment 7", [this]()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(testing::_, testing::_, testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalMotorAlignmentTest, force_alignment_short_command)
{
    InvokeCommand("fa 4", [this]()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(testing::_, testing::_, testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalMotorAlignmentTest, force_alignment_invalid_argument_count)
{
    InvokeCommand("force_alignment", [this]()
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

TEST_F(TerminalMotorAlignmentTest, force_alignment_invalid_pole_pairs)
{
    InvokeCommand("fa abc", [this]()
        {
            ::testing::InSequence _;

            std::string header{ "ERROR: " };
            std::string payload{ "invalid value. It should be an integer." };
            std::string newline{ "\r\n" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalMotorAlignmentTest, force_alignment_too_many_arguments)
{
    InvokeCommand("fa 7 extra", [this]()
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

TEST_F(TerminalMotorAlignmentTest, force_alignment_successful_callback)
{
    infra::Function<void(std::optional<foc::Radians>)> capturedCallback;

    InvokeCommand("fa 7", [this, &capturedCallback]()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(7, testing::_, testing::_))
                .WillOnce(testing::SaveArg<2>(&capturedCallback));
        });

    ExecuteAllActions();

    // Simulate successful alignment
    ::testing::InSequence _;
    std::string newline{ "\r\n" };
    std::string prefix{ "Motor aligned at position (rad): " };
    // The tracer outputs the float value, which we'll just accept any characters for
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(prefix.begin(), prefix.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AtLeast(1)); // Accept the numeric value output

    capturedCallback(foc::Radians{ 1.57f });
    ExecuteAllActions();
}

TEST_F(TerminalMotorAlignmentTest, force_alignment_failed_callback)
{
    infra::Function<void(std::optional<foc::Radians>)> capturedCallback;

    InvokeCommand("fa 4", [this, &capturedCallback]()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(4, testing::_, testing::_))
                .WillOnce(testing::SaveArg<2>(&capturedCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence _;
    std::string message{ "Motor alignment failed." };
    std::string newline{ "\r\n" };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(message.begin(), message.end())), testing::_));

    capturedCallback(std::nullopt);
    ExecuteAllActions();
}

TEST_F(TerminalMotorAlignmentTest, force_alignment_with_zero_pole_pairs)
{
    InvokeCommand("fa 0", [this]()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(0, testing::_, testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalMotorAlignmentTest, force_alignment_with_large_pole_pairs)
{
    InvokeCommand("fa 100", [this]()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(100, testing::_, testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalMotorAlignmentTest, force_alignment_verifies_default_config_used)
{
    services::MotorAlignment::AlignmentConfig capturedConfig;

    InvokeCommand("fa 5", [this, &capturedConfig]()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(5, testing::_, testing::_))
                .WillOnce(testing::SaveArg<1>(&capturedConfig));
        });

    ExecuteAllActions();

    services::MotorAlignment::AlignmentConfig defaultConfig;
    EXPECT_EQ(capturedConfig.testVoltagePercent, defaultConfig.testVoltagePercent);
    EXPECT_EQ(capturedConfig.samplingFrequency, defaultConfig.samplingFrequency);
    EXPECT_EQ(capturedConfig.maxSamples, defaultConfig.maxSamples);
    EXPECT_EQ(capturedConfig.settledThreshold, defaultConfig.settledThreshold);
    EXPECT_EQ(capturedConfig.settledCount, defaultConfig.settledCount);
}

TEST_F(TerminalMotorAlignmentTest, force_alignment_with_negative_number)
{
    InvokeCommand("fa -5", [this]()
        {
            ::testing::InSequence _;

            std::string header{ "ERROR: " };
            std::string payload{ "invalid value. It should be an integer." };
            std::string newline{ "\r\n" };

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(header.begin(), header.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(payload.begin(), payload.end())), testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalMotorAlignmentTest, force_alignment_with_decimal_number)
{
    InvokeCommand("fa 3.5", [this]()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(3, testing::_, testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalMotorAlignmentTest, force_alignment_with_extra_spaces_succeeds)
{
    InvokeCommand("fa  7", [this]()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(7, testing::_, testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalMotorAlignmentTest, force_alignment_with_maximum_uint32_value)
{
    InvokeCommand("fa 4294967295", [this]()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(4294967295u, testing::_, testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalMotorAlignmentTest, force_alignment_callback_with_zero_position)
{
    infra::Function<void(std::optional<foc::Radians>)> capturedCallback;

    InvokeCommand("fa 3", [this, &capturedCallback]()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(3, testing::_, testing::_))
                .WillOnce(testing::SaveArg<2>(&capturedCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence _;
    std::string newline{ "\r\n" };
    std::string prefix{ "Motor aligned at position (rad): " };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(prefix.begin(), prefix.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AtLeast(1));

    capturedCallback(foc::Radians{ 0.0f });
    ExecuteAllActions();
}

TEST_F(TerminalMotorAlignmentTest, force_alignment_command_can_be_called_multiple_times)
{
    InvokeCommand("fa 5", [this]()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(5, testing::_, testing::_));
        });

    ExecuteAllActions();

    InvokeCommand("force_alignment 10", [this]()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(10, testing::_, testing::_));
        });

    ExecuteAllActions();
}
