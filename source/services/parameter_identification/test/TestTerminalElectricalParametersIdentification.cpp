#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/test_helper/MockHelpers.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include "source/services/parameter_identification/ElectricalParametersIdentification.hpp"
#include "source/services/parameter_identification/TerminalElectricalParametersIdentification.hpp"
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

    class ElectricalParametersIdentificationMock
        : public services::ElectricalParametersIdentification
    {
    public:
        MOCK_METHOD2(EstimateResistanceAndInductance, void(const ResistanceAndInductanceConfig& config, const infra::Function<void(std::optional<foc::Ohm>, std::optional<foc::MilliHenry>)>& onDone));
        MOCK_METHOD2(EstimateNumberOfPolePairs, void(const PolePairsConfig& config, const infra::Function<void(std::optional<std::size_t>)>& onDone));
    };

    class TerminalElectricalParametersIdentificationTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        ::testing::StrictMock<ElectricalParametersIdentificationMock> identificationMock;
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
        services::TerminalElectricalParametersIdentification terminalIdentification{ terminalWithStorage, tracer, identificationMock };

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
    };
}

TEST_F(TerminalElectricalParametersIdentificationTest, estrl_calls_identification_with_wye)
{
    InvokeCommand("estrl wye", [this]()
        {
            EXPECT_CALL(identificationMock, EstimateResistanceAndInductance(testing::_, testing::_))
                .WillOnce([](const auto& config, const auto&)
                    {
                        EXPECT_EQ(config.windingConfig, services::WindingConfiguration::Wye);
                    });
        });

    ExecuteAllActions();
}

TEST_F(TerminalElectricalParametersIdentificationTest, estrl_calls_identification_with_delta)
{
    InvokeCommand("estrl delta", [this]()
        {
            EXPECT_CALL(identificationMock, EstimateResistanceAndInductance(testing::_, testing::_))
                .WillOnce([](const auto& config, const auto&)
                    {
                        EXPECT_EQ(config.windingConfig, services::WindingConfiguration::Delta);
                    });
        });

    ExecuteAllActions();
}

TEST_F(TerminalElectricalParametersIdentificationTest, estrl_successful_callback)
{
    infra::Function<void(std::optional<foc::Ohm>, std::optional<foc::MilliHenry>)> capturedCallback;

    InvokeCommand("estrl star", [this, &capturedCallback]()
        {
            EXPECT_CALL(identificationMock, EstimateResistanceAndInductance(testing::_, testing::_))
                .WillOnce(testing::SaveArg<1>(&capturedCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence _;
    std::string newline{ "\r\n" };
    std::string prefix{ "Estimated Resistance: " };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(prefix.begin(), prefix.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AtLeast(1));

    capturedCallback(foc::Ohm{ 1.5f }, foc::MilliHenry{ 2.0f });
    ExecuteAllActions();
}

TEST_F(TerminalElectricalParametersIdentificationTest, estrl_resistance_failure_callback)
{
    infra::Function<void(std::optional<foc::Ohm>, std::optional<foc::MilliHenry>)> capturedCallback;

    InvokeCommand("estimate_r_and_l wye", [this, &capturedCallback]()
        {
            EXPECT_CALL(identificationMock, EstimateResistanceAndInductance(testing::_, testing::_))
                .WillOnce(testing::SaveArg<1>(&capturedCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence _;
    std::string newline{ "\r\n" };
    std::string message{ "Resistance estimation failed." };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(message.begin(), message.end())), testing::_));

    capturedCallback(std::nullopt, std::nullopt);
    ExecuteAllActions();
}

TEST_F(TerminalElectricalParametersIdentificationTest, estpp_calls_identification)
{
    InvokeCommand("estpp", [this]()
        {
            EXPECT_CALL(identificationMock, EstimateNumberOfPolePairs(testing::_, testing::_));
        });

    ExecuteAllActions();
}

TEST_F(TerminalElectricalParametersIdentificationTest, estpp_successful_callback)
{
    infra::Function<void(std::optional<std::size_t>)> capturedCallback;

    InvokeCommand("estimate_pole_pairs", [this, &capturedCallback]()
        {
            EXPECT_CALL(identificationMock, EstimateNumberOfPolePairs(testing::_, testing::_))
                .WillOnce(testing::SaveArg<1>(&capturedCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence _;
    std::string newline{ "\r\n" };
    std::string prefix{ "Estimated Pole Pairs: " };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(prefix.begin(), prefix.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(testing::_, testing::_)).Times(testing::AtLeast(1));

    capturedCallback(std::make_optional<std::size_t>(7));
    ExecuteAllActions();
}

TEST_F(TerminalElectricalParametersIdentificationTest, estpp_failure_callback)
{
    infra::Function<void(std::optional<std::size_t>)> capturedCallback;

    InvokeCommand("estpp", [this, &capturedCallback]()
        {
            EXPECT_CALL(identificationMock, EstimateNumberOfPolePairs(testing::_, testing::_))
                .WillOnce(testing::SaveArg<1>(&capturedCallback));
        });

    ExecuteAllActions();

    ::testing::InSequence _;
    std::string newline{ "\r\n" };
    std::string message{ "Pole pairs estimation failed." };
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(newline.begin(), newline.end())), testing::_));
    EXPECT_CALL(streamWriterMock, Insert(infra::CheckByteRangeContents(std::vector<uint8_t>(message.begin(), message.end())), testing::_));

    capturedCallback(std::nullopt);
    ExecuteAllActions();
}
