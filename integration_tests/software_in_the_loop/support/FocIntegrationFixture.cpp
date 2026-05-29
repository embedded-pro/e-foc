#include "integration_tests/software_in_the_loop/support/FocIntegrationFixture.hpp"
#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"

namespace integration
{
    const foc::Volts FocIntegrationFixture::testVdc{ 24.0f };
    using namespace testing;

    FocIntegrationFixture::FocIntegrationFixture()
    {
        EXPECT_CALL(platformFactory, PhaseCurrentsReady(_, _)).Times(AnyNumber());
        EXPECT_CALL(platformFactory, ThreePhasePwmOutput(_)).Times(AnyNumber());
        EXPECT_CALL(platformFactory, BaseFrequency()).Times(AnyNumber()).WillRepeatedly(Return(hal::Hertz{ 10000 }));
        EXPECT_CALL(platformFactory, Start()).Times(AnyNumber());
        EXPECT_CALL(platformFactory, Stop()).Times(AnyNumber());
        EXPECT_CALL(platformFactory, Read()).Times(AnyNumber()).WillRepeatedly(Return(foc::Radians{ 0.0f }));
        EXPECT_CALL(platformFactory, Set(_)).Times(AnyNumber());
        EXPECT_CALL(platformFactory, SetZero()).Times(AnyNumber());
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
            application::MotorHardware{ platformFactory, platformFactory, testVdc },
            nvm,
            application::CalibrationServices{ electricalIdentMock, alignmentMock },
            faultNotifierMock,
            state_machine::TransitionPolicy::Auto);

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
            application::MotorHardware{ platformFactory, platformFactory, testVdc },
            nvm,
            application::CalibrationServices{ electricalIdentMock, alignmentMock },
            faultNotifierMock,
            state_machine::TransitionPolicy::Auto);

        ExecuteAllActions();
    }

    void FocIntegrationFixture::SetupCalibrationExpectations()
    {
        calibrationExpectationsConfigured = true;
        EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
            .Times(AnyNumber())
            .WillRepeatedly(Invoke([this](const auto&, const auto& cb)
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
        motorCategoryServer->SetAcknowledger(nullAcknowledger);
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

    void FocIntegrationFixture::InjectCanEmergencyStop()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        motorCategoryServer->HandleMessage(services::focEmergencyStopId, data);
        ExecuteAllActions();
    }

    void FocIntegrationFixture::DeferClearCalibration()
    {
        eepromStub.DeferNextErase();
        motorStateMachine->CmdClearCalibration([](state_machine::CommandResult) {});
        ExecuteAllActions();
    }

    void FocIntegrationFixture::CompleteInvalidate(services::NvmStatus /* status */)
    {
        eepromStub.CompleteDeferredErase();
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
