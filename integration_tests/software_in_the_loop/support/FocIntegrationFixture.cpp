#include "integration_tests/software_in_the_loop/support/FocIntegrationFixture.hpp"
#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"

namespace integration
{
    const foc::Volts FocIntegrationFixture::testVdc{ 24.0f };
    using namespace testing;

    FocIntegrationFixture::FocIntegrationFixture()
    {
        EXPECT_CALL(platformFactory, AdcMultiChannelCreator()).WillRepeatedly(ReturnRef(adcCreator));
        EXPECT_CALL(platformFactory, SynchronousThreeChannelsPwmCreator()).WillRepeatedly(ReturnRef(pwmCreator));
        EXPECT_CALL(platformFactory, SynchronousQuadratureEncoderCreator()).WillRepeatedly(ReturnRef(encoderCreator));

        EXPECT_CALL(adcCreator, Constructed(application::PlatformFactory::SampleAndHold::shorter));
        EXPECT_CALL(pwmCreator, Constructed(std::chrono::nanoseconds{ 500 }, hal::Hertz{ 10000 }));
        EXPECT_CALL(encoderCreator, Constructed());

        EXPECT_CALL(adcCreator, Destructed()).Times(AtLeast(1));
        EXPECT_CALL(pwmCreator, Destructed()).Times(AtLeast(1));
        EXPECT_CALL(encoderCreator, Destructed()).Times(AtLeast(1));

        EXPECT_CALL(pwmMock, SetBaseFrequency(_)).Times(AnyNumber());
        EXPECT_CALL(pwmMock, Start(_, _, _)).Times(AnyNumber());
        EXPECT_CALL(pwmMock, Stop()).Times(AnyNumber());
        EXPECT_CALL(adcPhaseCurrentMock, Measure(_)).Times(AnyNumber());
        EXPECT_CALL(adcPhaseCurrentMock, Stop()).Times(AnyNumber());
        EXPECT_CALL(encoderDecoratorMock, Read()).Times(AnyNumber()).WillRepeatedly(Return(foc::Radians{ 0.0f }));

        platformAdapter.emplace(platformFactory);
    }

    void FocIntegrationFixture::ConstructWithInvalidNvm()
    {
        EXPECT_CALL(faultNotifierMock, Register(_))
            .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
                {
                    faultNotifierMock.StoreHandler(handler);
                }));

        motorStateMachine.emplace(
            application::TerminalAndTracer{ terminal, tracer },
            application::MotorHardware{ *platformAdapter, *platformAdapter, testVdc },
            nvm,
            application::CalibrationServices{ electricalIdentMock, alignmentMock },
            faultNotifierMock);

        ExecuteAllActions();
    }

    void FocIntegrationFixture::ConstructWithValidNvm(services::CalibrationData data)
    {
        bool saved = false;
        nvm.SaveCalibration(data, [&saved](services::NvmStatus)
            {
                saved = true;
            });
        ExecuteAllActions();
        EXPECT_TRUE(saved);

        EXPECT_CALL(faultNotifierMock, Register(_))
            .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
                {
                    faultNotifierMock.StoreHandler(handler);
                }));

        motorStateMachine.emplace(
            application::TerminalAndTracer{ terminal, tracer },
            application::MotorHardware{ *platformAdapter, *platformAdapter, testVdc },
            nvm,
            application::CalibrationServices{ electricalIdentMock, alignmentMock },
            faultNotifierMock);

        ExecuteAllActions();
    }

    void FocIntegrationFixture::SetupCalibrationExpectations()
    {
        calibrationExpectationsConfigured = true;
        EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
            .WillOnce(Invoke([this](const auto&, const auto& cb)
                {
                    capturedPolePairsCallback = cb;
                }));
    }

    void FocIntegrationFixture::SetupCanIntegration()
    {
        EXPECT_CALL(transportCanMock, SendData(_, _, _))
            .Times(AnyNumber())
            .WillRepeatedly(Invoke([](auto, const auto&, const auto& cb)
                {
                    cb(true);
                }));

        canTransport.emplace(transportCanMock, 1);
        motorCategoryServer.emplace(*canTransport);
        motorBridge.emplace(*motorCategoryServer, *motorStateMachine);
    }

    void FocIntegrationFixture::InjectCanStart()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        motorCategoryServer->HandleMessage(services::focStartId, data);
        ExecuteAllActions();
    }

    void FocIntegrationFixture::InjectCanStop()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        motorCategoryServer->HandleMessage(services::focStopId, data);
        ExecuteAllActions();
    }

    void FocIntegrationFixture::InjectCanClearFault()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        motorCategoryServer->HandleMessage(services::focClearFaultId, data);
        ExecuteAllActions();
    }

    void FocIntegrationFixture::CompletePolePairsEstimation(std::size_t polePairs)
    {
        EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
            .WillOnce(Invoke([this](const auto&, const auto& cb)
                {
                    capturedRLCallback = cb;
                }));

        capturedPolePairsCallback(std::optional<std::size_t>{ polePairs });
        ExecuteAllActions();
    }

    void FocIntegrationFixture::CompleteRLEstimation(foc::Ohm resistance, foc::MilliHenry inductance)
    {
        EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
            .WillOnce(Invoke([this](auto, const auto&, const auto& cb)
                {
                    capturedAlignmentCallback = cb;
                }));

        capturedRLCallback(std::optional<foc::Ohm>{ resistance }, std::optional<foc::MilliHenry>{ inductance });
        ExecuteAllActions();
    }

    void FocIntegrationFixture::CompleteAlignment(foc::Radians offset)
    {
        capturedAlignmentCallback(std::optional<foc::Radians>{ offset });
        ExecuteAllActions();
    }

    services::CalibrationData FocIntegrationFixture::MakeDefaultCalibrationData()
    {
        services::CalibrationData data{};
        data.polePairs = 7;
        data.rPhase = 0.5f;
        data.lD = 1.0f;
        data.lQ = 1.0f;
        return data;
    }
}
