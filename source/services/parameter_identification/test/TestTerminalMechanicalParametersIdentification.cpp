#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/test_helper/MockHelpers.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include "source/services/parameter_identification/MechanicalParametersIdentification.hpp"
#include "source/services/parameter_identification/TerminalMechanicalParametersIdentification.hpp"
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

    class MechanicalParametersIdentificationMock
        : public services::MechanicalParametersIdentification
    {
    public:
        MOCK_METHOD4(EstimateFrictionAndInertia, void(const foc::NewtonMeter& torqueConstant, std::size_t numberOfPolePairs, const Config& config, const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)>& onDone));
    };

    class TerminalMechanicalParametersIdentificationTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        ::testing::StrictMock<MechanicalParametersIdentificationMock> identificationMock;
        ::testing::StrictMock<StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriterMock };
        services::TracerToStream tracer{ stream };
        ::testing::StrictMock<hal::SerialCommunicationMock> communication;
        infra::Execute execute{ [this]()
            {
                EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { '>', ' ' } }), testing::_));
            } };
        services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<128, 5> terminalWithCommands{ communication, tracer };
        services::TerminalWithStorage::WithMaxSize<16> terminalWithStorage{ terminalWithCommands, tracer };
        services::TerminalMechanicalParametersIdentification terminalIdentification{ terminalWithStorage, tracer, identificationMock };

        void InvokeCommand(const std::string& command, const infra::Function<void()>& onCommandReceived)
        {
            ::testing::InSequence _;

            for (const auto& data : command)
                EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ static_cast<uint8_t>(data) }), testing::_));

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { '\r', '\n' } }), testing::_));
            onCommandReceived();
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { '>', ' ' } }), testing::_));

            communication.dataReceived(infra::MakeStringByteRange(command + "\r"));
        }

        void InvokeCommandExpectingError(const std::string& command, const std::string& errorMessage)
        {
            ::testing::InSequence _;

            for (const auto& data : command)
                EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ static_cast<uint8_t>(data) }), testing::_));

            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { '\r', '\n' } }), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { '\r', '\n' } }), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { 'E', 'R', 'R', 'O', 'R', ':', ' ' } }), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(errorMessage.begin(), errorMessage.end())), testing::_));
            EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>{ { '>', ' ' } }), testing::_));

            communication.dataReceived(infra::MakeStringByteRange(command + "\r"));
        }
    };
}

TEST_F(TerminalMechanicalParametersIdentificationTest, estfric_calls_identification_with_correct_parameters)
{
    InvokeCommand("estmech 500.0 0.1 7", [this]()
        {
            EXPECT_CALL(identificationMock, EstimateFrictionAndInertia(testing::_, testing::_, testing::_, testing::_))
                .WillOnce([](const auto& torqueConstant, std::size_t polePairs, const auto& config, const auto&)
                    {
                        float expectedSpeed = 500.0f * 2.0f * 3.14159265f / 60.0f; // RPM to rad/s
                        EXPECT_NEAR(config.targetSpeed.Value(), expectedSpeed, 0.1f);
                        EXPECT_FLOAT_EQ(torqueConstant.Value(), 0.1f);
                        EXPECT_EQ(polePairs, 7);
                    });
        });

    ExecuteAllActions();
}

TEST_F(TerminalMechanicalParametersIdentificationTest, estfric_returns_error_for_invalid_arguments)
{
    InvokeCommandExpectingError("estmech 500.0 0.1", "invalid number of arguments. Usage: estmech <target_speed_rpm> <Kt_Nm_per_A> <number_of_pole_pairs>");

    ExecuteAllActions();
}

TEST_F(TerminalMechanicalParametersIdentificationTest, estfric_successful_callback)
{
    infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)> capturedCallback;

    InvokeCommand("estmech 300.0 0.15 7", [this, &capturedCallback]()
        {
            EXPECT_CALL(identificationMock, EstimateFrictionAndInertia(testing::_, testing::_, testing::_, testing::_))
                .WillOnce(testing::SaveArg<3>(&capturedCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence _;
    std::string newline{ "\r\n" };
    std::string prefix1{ "Estimated Friction (B): " };
    std::string prefix2{ "Estimated Inertia (J): " };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(prefix1.begin(), prefix1.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AtLeast(1));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(prefix2.begin(), prefix2.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AtLeast(1));

    capturedCallback(foc::NewtonMeterSecondPerRadian{ 0.05f }, foc::NewtonMeterSecondSquared{ 0.001f });
    ExecuteAllActions();
}

TEST_F(TerminalMechanicalParametersIdentificationTest, estfric_failure_callback)
{
    infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)> capturedCallback;

    InvokeCommand("estmech 400.0 0.1 7", [this, &capturedCallback]()
        {
            EXPECT_CALL(identificationMock, EstimateFrictionAndInertia(testing::_, testing::_, testing::_, testing::_))
                .WillOnce(testing::SaveArg<3>(&capturedCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence _;
    std::string newline{ "\r\n" };
    std::string message{ "Friction and inertia estimation failed." };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(message.begin(), message.end())), testing::_));

    capturedCallback(std::nullopt, std::nullopt);
    ExecuteAllActions();
}
