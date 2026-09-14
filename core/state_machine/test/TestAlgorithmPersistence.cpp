#include "TestFocStateMachineHelper.hpp"
#include "core/foc/interfaces/test_doubles/FocMock.hpp"
#include "core/state_machine/AlgorithmPersistence.hpp"

namespace
{
    using namespace testing;

    class AlgorithmPersistenceTest
        : public ::testing::Test
    {
    public:
        StrictMock<services::NonVolatileMemoryMock> nvm;
        infra::StreamWriterMock streamWriter;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriter };
        services::TracerToStream tracer{ stream };
        services::ConfigData configData{};
        state_machine::AlgorithmPersistence persistence{ nvm, configData, tracer };

        StrictMock<foc::FocTorqueMock> currentMock;
        StrictMock<foc::FocSpeedMock> speedMock;
        StrictMock<foc::FocPositionMock> positionMock;

        infra::Execute suppressTracerExpectations{ [this]()
            {
                EXPECT_CALL(streamWriter, Insert(_, _)).Times(AnyNumber());
                EXPECT_CALL(streamWriter, Available()).Times(AnyNumber()).WillRepeatedly(Return(1000));
                EXPECT_CALL(streamWriter, ConstructSaveMarker()).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriter, GetProcessedBytesSince(_)).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriter, SaveState(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
                EXPECT_CALL(streamWriter, RestoreState(_)).Times(AnyNumber());
                EXPECT_CALL(streamWriter, Overwrite(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
            } };
    };
}

TEST_F(AlgorithmPersistenceTest, apply_persisted_current_algorithm_valid_raw_selects_it)
{
    configData.currentAlgorithm = static_cast<uint8_t>(foc::CurrentAlgorithm::deadbeat);
    EXPECT_CALL(currentMock, ActiveCurrentAlgorithm()).WillOnce(Return(foc::CurrentAlgorithm::pid));
    EXPECT_CALL(currentMock, SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat)).WillOnce(Return(foc::SelectResult::ok));

    persistence.ApplyPersistedAlgorithms(&currentMock, nullptr, nullptr);
}

TEST_F(AlgorithmPersistenceTest, apply_persisted_already_active_algorithm_skips_select)
{
    configData.currentAlgorithm = static_cast<uint8_t>(foc::CurrentAlgorithm::pid);
    EXPECT_CALL(currentMock, ActiveCurrentAlgorithm()).WillOnce(Return(foc::CurrentAlgorithm::pid));

    persistence.ApplyPersistedAlgorithms(&currentMock, nullptr, nullptr);
}

TEST_F(AlgorithmPersistenceTest, apply_persisted_invalid_raw_falls_back_to_active_and_updates_config)
{
    configData.currentAlgorithm = 99;
    EXPECT_CALL(currentMock, ActiveCurrentAlgorithm()).WillOnce(Return(foc::CurrentAlgorithm::pid));

    persistence.ApplyPersistedAlgorithms(&currentMock, nullptr, nullptr);

    EXPECT_EQ(configData.currentAlgorithm, static_cast<uint8_t>(foc::CurrentAlgorithm::pid));
}

TEST_F(AlgorithmPersistenceTest, select_current_algorithm_succeeds_updates_config_and_persists)
{
    EXPECT_CALL(currentMock, SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat)).WillOnce(Return(foc::SelectResult::ok));
    EXPECT_CALL(nvm, SaveConfig(_, _));

    const auto result = persistence.SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat, &currentMock);

    EXPECT_EQ(result, foc::SelectResult::ok);
    EXPECT_EQ(configData.currentAlgorithm, static_cast<uint8_t>(foc::CurrentAlgorithm::deadbeat));
}

TEST_F(AlgorithmPersistenceTest, select_current_algorithm_failure_does_not_persist)
{
    EXPECT_CALL(currentMock, SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat)).WillOnce(Return(foc::SelectResult::invalidAlgorithm));

    const auto result = persistence.SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat, &currentMock);

    EXPECT_EQ(result, foc::SelectResult::invalidAlgorithm);
}

TEST_F(AlgorithmPersistenceTest, select_current_algorithm_null_selectable_returns_invalid)
{
    const auto result = persistence.SelectCurrentAlgorithm(foc::CurrentAlgorithm::pid, nullptr);
    EXPECT_EQ(result, foc::SelectResult::invalidAlgorithm);
}

TEST_F(AlgorithmPersistenceTest, select_speed_algorithm_succeeds)
{
    EXPECT_CALL(speedMock, SelectSpeedAlgorithm(foc::SpeedAlgorithm::lqi)).WillOnce(Return(foc::SelectResult::ok));
    EXPECT_CALL(nvm, SaveConfig(_, _));

    const auto result = persistence.SelectSpeedAlgorithm(foc::SpeedAlgorithm::lqi, &speedMock);

    EXPECT_EQ(result, foc::SelectResult::ok);
    EXPECT_EQ(configData.speedAlgorithm, static_cast<uint8_t>(foc::SpeedAlgorithm::lqi));
}

TEST_F(AlgorithmPersistenceTest, select_position_algorithm_succeeds)
{
    EXPECT_CALL(positionMock, SelectPositionAlgorithm(foc::PositionAlgorithm::lqr)).WillOnce(Return(foc::SelectResult::ok));
    EXPECT_CALL(nvm, SaveConfig(_, _));

    const auto result = persistence.SelectPositionAlgorithm(foc::PositionAlgorithm::lqr, &positionMock);

    EXPECT_EQ(result, foc::SelectResult::ok);
    EXPECT_EQ(configData.positionAlgorithm, static_cast<uint8_t>(foc::PositionAlgorithm::lqr));
}

TEST_F(AlgorithmPersistenceTest, active_current_algorithm_reads_from_selectable)
{
    EXPECT_CALL(currentMock, ActiveCurrentAlgorithm()).WillOnce(Return(foc::CurrentAlgorithm::deadbeat));
    EXPECT_EQ(persistence.ActiveCurrentAlgorithm(&currentMock), foc::CurrentAlgorithm::deadbeat);
}

TEST_F(AlgorithmPersistenceTest, active_current_algorithm_falls_back_to_config)
{
    configData.currentAlgorithm = static_cast<uint8_t>(foc::CurrentAlgorithm::slidingMode);
    EXPECT_EQ(persistence.ActiveCurrentAlgorithm(nullptr), foc::CurrentAlgorithm::slidingMode);
}

TEST_F(AlgorithmPersistenceTest, current_algorithm_name_round_trips)
{
    EXPECT_STREQ(state_machine::AlgorithmPersistence::CurrentAlgorithmName(foc::CurrentAlgorithm::pid), "pid");
    EXPECT_STREQ(state_machine::AlgorithmPersistence::CurrentAlgorithmName(foc::CurrentAlgorithm::decoupledPid), "decoupled");
    EXPECT_STREQ(state_machine::AlgorithmPersistence::CurrentAlgorithmName(foc::CurrentAlgorithm::deadbeat), "deadbeat");
    EXPECT_STREQ(state_machine::AlgorithmPersistence::CurrentAlgorithmName(foc::CurrentAlgorithm::slidingMode), "sliding");
}

TEST_F(AlgorithmPersistenceTest, speed_algorithm_name_round_trips)
{
    EXPECT_STREQ(state_machine::AlgorithmPersistence::SpeedAlgorithmName(foc::SpeedAlgorithm::pid), "pid");
    EXPECT_STREQ(state_machine::AlgorithmPersistence::SpeedAlgorithmName(foc::SpeedAlgorithm::lqi), "lqi");
    EXPECT_STREQ(state_machine::AlgorithmPersistence::SpeedAlgorithmName(foc::SpeedAlgorithm::adrc), "adrc");
    EXPECT_STREQ(state_machine::AlgorithmPersistence::SpeedAlgorithmName(foc::SpeedAlgorithm::twoDof), "twodof");
}

TEST_F(AlgorithmPersistenceTest, position_algorithm_name_round_trips)
{
    EXPECT_STREQ(state_machine::AlgorithmPersistence::PositionAlgorithmName(foc::PositionAlgorithm::pid), "pid");
    EXPECT_STREQ(state_machine::AlgorithmPersistence::PositionAlgorithmName(foc::PositionAlgorithm::lqr), "lqr");
    EXPECT_STREQ(state_machine::AlgorithmPersistence::PositionAlgorithmName(foc::PositionAlgorithm::lqi), "lqi");
    EXPECT_STREQ(state_machine::AlgorithmPersistence::PositionAlgorithmName(foc::PositionAlgorithm::twoDof), "twodof");
}
