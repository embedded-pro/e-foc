#include "integration_tests/software_in_the_loop/support/PositionIntegrationFixture.hpp"
#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"

namespace integration
{
    const foc::Volts PositionIntegrationFixture::testVdc{ 24.0f };
    using namespace testing;

    PositionIntegrationFixture::PositionIntegrationFixture()
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
        EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());

        platformAdapter.emplace(platformFactory);
    }

    void PositionIntegrationFixture::ConstructWithInvalidNvm()
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
            application::CalibrationServices{ electricalIdentMock, alignmentMock, &mechIdentMock },
            faultNotifierMock,
            foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock);

        ExecuteAllActions();
    }

    void PositionIntegrationFixture::ConstructWithValidNvm(services::CalibrationData data)
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
            application::CalibrationServices{ electricalIdentMock, alignmentMock, &mechIdentMock },
            faultNotifierMock,
            foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock);

        ExecuteAllActions();
    }

    void PositionIntegrationFixture::SetupCalibrationExpectations()
    {
        calibrationExpectationsConfigured = true;
        EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
            .WillOnce(Invoke([this](const auto&, const auto& cb)
                {
                    capturedPolePairsCallback = cb;
                }));
    }

    void PositionIntegrationFixture::SetupCanIntegration()
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

    void PositionIntegrationFixture::InjectCanStart()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        motorCategoryServer->HandleMessage(services::focStartId, data);
        ExecuteAllActions();
    }

    void PositionIntegrationFixture::InjectCanStop()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        motorCategoryServer->HandleMessage(services::focStopId, data);
        ExecuteAllActions();
    }

    void PositionIntegrationFixture::InjectCanClearFault()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        motorCategoryServer->HandleMessage(services::focClearFaultId, data);
        ExecuteAllActions();
    }

    void PositionIntegrationFixture::CompletePolePairsEstimation(std::size_t polePairs)
    {
        EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
            .WillOnce(Invoke([this](const auto&, const auto& cb)
                {
                    capturedRLCallback = cb;
                }));

        capturedPolePairsCallback(std::optional<std::size_t>{ polePairs });
        ExecuteAllActions();
    }

    void PositionIntegrationFixture::CompleteRLEstimation(foc::Ohm resistance, foc::MilliHenry inductance)
    {
        EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
            .WillOnce(Invoke([this](auto, const auto&, const auto& cb)
                {
                    capturedAlignmentCallback = cb;
                }));

        capturedRLCallback(std::optional<foc::Ohm>{ resistance }, std::optional<foc::MilliHenry>{ inductance });
        ExecuteAllActions();
    }

    void PositionIntegrationFixture::CompleteAlignment(foc::Radians offset)
    {
        EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
            .WillOnce(Invoke([this](const auto&, auto, const auto&, const auto& cb)
                {
                    capturedMechIdentCallback = cb;
                }));

        capturedAlignmentCallback(std::optional<foc::Radians>{ offset });
        ExecuteAllActions();
    }

    void PositionIntegrationFixture::CompleteMechanicalIdentification(
        std::optional<foc::NewtonMeterSecondPerRadian> friction,
        std::optional<foc::NewtonMeterSecondSquared> inertia)
    {
        capturedMechIdentCallback(friction, inertia);
        ExecuteAllActions();
    }

    services::CalibrationData PositionIntegrationFixture::MakeDefaultCalibrationData()
    {
        services::CalibrationData data{};
        data.polePairs = 7;
        data.rPhase = 0.5f;
        data.lD = 1.0f;
        data.lQ = 1.0f;
        data.kpVelocity = 0.25f;
        data.kiVelocity = 0.5f;
        return data;
    }
}
