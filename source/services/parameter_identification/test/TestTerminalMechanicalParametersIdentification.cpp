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
        MOCK_METHOD2(EstimateFriction, void(const FrictionConfig& config, const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>)>& onDone));
        MOCK_METHOD3(EstimateInertia, void(const InertiaConfig& config, foc::NewtonMeterSecondPerRadian damping, const infra::Function<void(std::optional<foc::NewtonMeterSecondSquared>)>& onDone));
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
    InvokeCommand("estfric 500.0 0.1", [this]()
        {
            EXPECT_CALL(identificationMock, EstimateFriction(testing::_, testing::_))
                .WillOnce([](const auto& config, const auto&)
                    {
                        float expectedSpeed = 500.0f * 2.0f * 3.14159265f / 60.0f; // RPM to rad/s
                        EXPECT_NEAR(config.targetSpeed.Value(), expectedSpeed, 0.1f);
                        EXPECT_FLOAT_EQ(config.torqueConstant.Value(), 0.1f);
                    });
        });

    ExecuteAllActions();
}

TEST_F(TerminalMechanicalParametersIdentificationTest, estfric_returns_error_for_invalid_arguments)
{
    InvokeCommandExpectingError("estfric 500.0", "invalid number of arguments. Usage: estfric <target_speed_rpm> <Kt_Nm_per_A>");

    ExecuteAllActions();
}

TEST_F(TerminalMechanicalParametersIdentificationTest, estfric_successful_callback)
{
    infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>)> capturedCallback;

    InvokeCommand("estimate_friction 300.0 0.15", [this, &capturedCallback]()
        {
            EXPECT_CALL(identificationMock, EstimateFriction(testing::_, testing::_))
                .WillOnce(testing::SaveArg<1>(&capturedCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence _;
    std::string newline{ "\r\n" };
    std::string prefix{ "Estimated Friction (B): " };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(prefix.begin(), prefix.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AtLeast(1));

    capturedCallback(foc::NewtonMeterSecondPerRadian{ 0.05f });
    ExecuteAllActions();
}

TEST_F(TerminalMechanicalParametersIdentificationTest, estfric_failure_callback)
{
    infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>)> capturedCallback;

    InvokeCommand("estfric 400.0 0.1", [this, &capturedCallback]()
        {
            EXPECT_CALL(identificationMock, EstimateFriction(testing::_, testing::_))
                .WillOnce(testing::SaveArg<1>(&capturedCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence _;
    std::string newline{ "\r\n" };
    std::string message{ "Friction estimation failed." };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(message.begin(), message.end())), testing::_));

    capturedCallback(std::nullopt);
    ExecuteAllActions();
}

TEST_F(TerminalMechanicalParametersIdentificationTest, estinert_requires_friction_to_be_estimated_first)
{
    InvokeCommandExpectingError("estinert 1.0 0.1", "friction must be estimated first. Run 'estfric' command.");

    ExecuteAllActions();
}

TEST_F(TerminalMechanicalParametersIdentificationTest, estinert_calls_identification_after_friction_estimated)
{
    infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>)> frictionCallback;

    InvokeCommand("estfric 500.0 0.1", [this, &frictionCallback]()
        {
            EXPECT_CALL(identificationMock, EstimateFriction(testing::_, testing::_))
                .WillOnce(testing::SaveArg<1>(&frictionCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence _;
    std::string newline{ "\r\n" };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AtLeast(1));

    frictionCallback(foc::NewtonMeterSecondPerRadian{ 0.05f });
    ExecuteAllActions();

    InvokeCommand("estimate_inertia 2.0 0.1", [this]()
        {
            EXPECT_CALL(identificationMock, EstimateInertia(testing::_, testing::_, testing::_))
                .WillOnce([](const auto& config, auto damping, const auto&)
                    {
                        EXPECT_FLOAT_EQ(config.torqueStepCurrent.Value(), 2.0f);
                        EXPECT_FLOAT_EQ(config.torqueConstant.Value(), 0.1f);
                        EXPECT_FLOAT_EQ(damping.Value(), 0.05f);
                    });
        });

    ExecuteAllActions();
}

TEST_F(TerminalMechanicalParametersIdentificationTest, estinert_successful_callback)
{
    infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>)> frictionCallback;

    InvokeCommand("estfric 500.0 0.1", [this, &frictionCallback]()
        {
            EXPECT_CALL(identificationMock, EstimateFriction(testing::_, testing::_))
                .WillOnce(testing::SaveArg<1>(&frictionCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence s1;
    std::string newline1{ "\r\n" };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline1.begin(), newline1.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AtLeast(1));

    frictionCallback(foc::NewtonMeterSecondPerRadian{ 0.05f });
    ExecuteAllActions();

    infra::Function<void(std::optional<foc::NewtonMeterSecondSquared>)> inertiaCallback;

    InvokeCommand("estinert 1.5 0.1", [this, &inertiaCallback]()
        {
            EXPECT_CALL(identificationMock, EstimateInertia(testing::_, testing::_, testing::_))
                .WillOnce(testing::SaveArg<2>(&inertiaCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence s2;
    std::string newline2{ "\r\n" };
    std::string prefix{ "Estimated Inertia (J): " };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline2.begin(), newline2.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(prefix.begin(), prefix.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AtLeast(1));

    inertiaCallback(foc::NewtonMeterSecondSquared{ 0.001f });
    ExecuteAllActions();
}

TEST_F(TerminalMechanicalParametersIdentificationTest, estinert_failure_callback)
{
    infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>)> frictionCallback;

    InvokeCommand("estfric 500.0 0.1", [this, &frictionCallback]()
        {
            EXPECT_CALL(identificationMock, EstimateFriction(testing::_, testing::_))
                .WillOnce(testing::SaveArg<1>(&frictionCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence s1;
    std::string newline1{ "\r\n" };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline1.begin(), newline1.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AtLeast(1));

    frictionCallback(foc::NewtonMeterSecondPerRadian{ 0.05f });
    ExecuteAllActions();

    infra::Function<void(std::optional<foc::NewtonMeterSecondSquared>)> inertiaCallback;

    InvokeCommand("estinert 1.0 0.1", [this, &inertiaCallback]()
        {
            EXPECT_CALL(identificationMock, EstimateInertia(testing::_, testing::_, testing::_))
                .WillOnce(testing::SaveArg<2>(&inertiaCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence s2;
    std::string newline2{ "\r\n" };
    std::string message{ "Inertia estimation failed." };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline2.begin(), newline2.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(message.begin(), message.end())), testing::_));

    inertiaCallback(std::nullopt);
    ExecuteAllActions();
}
