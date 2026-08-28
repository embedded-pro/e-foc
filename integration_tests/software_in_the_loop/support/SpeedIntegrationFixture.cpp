#include "integration_tests/software_in_the_loop/support/SpeedIntegrationFixture.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "core/can/FocMotorMessages.hpp"

namespace sil
{
    const foc::Volts SpeedIntegrationFixture::testVdc{ 24.0f };
    using namespace testing;

    SpeedIntegrationFixture::SpeedIntegrationFixture()
    {

        EXPECT_CALL(platformFactory, PhaseCurrentsReady(_, _)).Times(AnyNumber());
        EXPECT_CALL(platformFactory, ThreePhasePwmOutput(_)).Times(AnyNumber());
        EXPECT_CALL(platformFactory, BaseFrequency()).Times(AnyNumber()).WillRepeatedly(Return(hal::Hertz{ 20000 }));
        EXPECT_CALL(platformFactory, Start()).Times(AnyNumber());
        EXPECT_CALL(platformFactory, Stop()).Times(AnyNumber());
        EXPECT_CALL(platformFactory, Read()).Times(AnyNumber()).WillRepeatedly(Return(foc::Radians{ 0.0f }));
        EXPECT_CALL(platformFactory, Set(_)).Times(AnyNumber());
        EXPECT_CALL(platformFactory, SetZero()).Times(AnyNumber());
        EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
        EXPECT_CALL(lowPriorityInterruptMock, Unregister()).Times(AnyNumber());
    }

    void SpeedIntegrationFixture::ConstructWithInvalidNvm()
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
            application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock) },
            faultNotifierMock,
            state_machine::TransitionPolicy::Auto,
            application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock });

        ExecuteAllActions();
    }

    void SpeedIntegrationFixture::ConstructWithValidNvm(services::CalibrationData data)
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
            application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock) },
            faultNotifierMock,
            state_machine::TransitionPolicy::Auto,
            application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock });

        ExecuteAllActions();
    }

    void SpeedIntegrationFixture::SetupCalibrationExpectations()
    {
        calibrationExpectationsConfigured = true;
        EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
            .Times(AnyNumber())
            .WillRepeatedly(Invoke([this](const auto&, const auto& cb)
                {
                    capturedPolePairsCallback = cb;
                }));
    }

    void SpeedIntegrationFixture::SetupCanIntegration()
    {
        EXPECT_CALL(transportCanMock, SendData(_, _, _))
            .Times(AnyNumber())
            .WillRepeatedly(Invoke([](auto, const auto&, const auto& cb)
                {
                    cb(true);
                }));

        EXPECT_CALL(transportCanMock, ReceiveData(_)).Times(AnyNumber());

        canProtocolServer.emplace(transportCanMock, services::CanProtocolServer::Config{ .nodeId = 1 });
        motorCategoryServer.emplace(canProtocolServer->Transport());
        canProtocolServer->RegisterCategory(*motorCategoryServer);
        motorBridge.emplace(*motorCategoryServer, *motorStateMachine);
    }

    void SpeedIntegrationFixture::InjectCanStart()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        motorCategoryServer->HandleMessage(can::focStartId, data);
        ExecuteAllActions();
    }

    void SpeedIntegrationFixture::InjectCanStop()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        motorCategoryServer->HandleMessage(can::focStopId, data);
        ExecuteAllActions();
    }

    void SpeedIntegrationFixture::InjectCanClearFault()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        motorCategoryServer->HandleMessage(can::focClearFaultId, data);
        ExecuteAllActions();
    }

    void SpeedIntegrationFixture::InjectCanEmergencyStop()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        motorCategoryServer->HandleMessage(can::focEmergencyStopId, data);
        ExecuteAllActions();
    }

    void SpeedIntegrationFixture::DeferClearCalibration()
    {
        eepromStub.DeferNextErase();
        motorStateMachine->CmdClearCalibration([](state_machine::CommandResult) {});
        ExecuteAllActions();
    }

    void SpeedIntegrationFixture::CompleteInvalidate(services::NvmStatus /* status */)
    {
        eepromStub.CompleteDeferredErase();
        ExecuteAllActions();
    }

    void SpeedIntegrationFixture::CompletePolePairsEstimation(std::size_t polePairs)
    {
        EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
            .WillOnce(Invoke([this](const auto&, const auto& cb)
                {
                    capturedRLCallback = cb;
                }));

        capturedPolePairsCallback(std::optional<std::size_t>{ polePairs });
        ExecuteAllActions();
    }

    void SpeedIntegrationFixture::CompleteRLEstimation(foc::Ohm resistance, foc::MilliHenry inductance)
    {
        EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
            .WillOnce(Invoke([this](auto, const auto&, const auto& cb)
                {
                    capturedAlignmentCallback = cb;
                }));

        capturedRLCallback(std::optional<foc::Ohm>{ resistance }, std::optional<foc::MilliHenry>{ inductance });
        ExecuteAllActions();
    }

    void SpeedIntegrationFixture::CompleteAlignment(foc::Radians offset)
    {
        EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
            .WillOnce(Invoke([this](const auto&, auto, const auto&, const auto& cb)
                {
                    capturedMechIdentCallback = cb;
                }));

        capturedAlignmentCallback(std::optional<foc::Radians>{ offset });
        ExecuteAllActions();
    }

    void SpeedIntegrationFixture::CompleteMechanicalIdentification(
        std::optional<foc::NewtonMeterSecondPerRadian> friction,
        std::optional<foc::NewtonMeterSecondSquared> inertia)
    {
        capturedMechIdentCallback(friction, inertia);
        ExecuteAllActions();
    }

    services::CalibrationData SpeedIntegrationFixture::MakeDefaultCalibrationData()
    {
        services::CalibrationData data{};
        data.polePairs = 7;
        data.rPhase = 0.5f;
        data.lD = 1.0f;
        data.lQ = 1.0f;
        data.speedLoopBandwidth = 50.0f;
        return data;
    }
}
