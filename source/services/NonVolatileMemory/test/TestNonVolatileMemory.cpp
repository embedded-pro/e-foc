#include "hal/interfaces/test_doubles/FlashStub.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/Crc.hpp"
#include "source/services/NonVolatileMemory/NonVolatileMemoryImpl.hpp"
#include <cstring>
#include <gmock/gmock.h>

namespace
{
    using namespace testing;

    static constexpr uint32_t sectorSize =
        std::max(sizeof(services::CalibrationStorageRecord),
            sizeof(services::ConfigStorageRecord)) +
        64u;

    static_assert(sectorSize >= sizeof(services::CalibrationStorageRecord));
    static_assert(sectorSize >= sizeof(services::ConfigStorageRecord));

    class NonVolatileMemoryTest
        : public ::testing::Test
        , public infra::EventDispatcherFixture
    {
    protected:
        hal::FlashStub flash{ 2, sectorSize };
        services::NonVolatileMemoryImpl nvm{ flash };

        services::CalibrationData MakeTestCalibration()
        {
            services::CalibrationData d{};
            d.rPhase = 1.5f;
            d.lD = 0.001f;
            d.lQ = 0.0012f;
            d.currentOffsetA = 0.01f;
            d.currentOffsetB = -0.01f;
            d.currentOffsetC = 0.005f;
            d.inertia = 0.0002f;
            d.frictionCoulomb = 0.05f;
            d.frictionViscous = 0.001f;
            d.encoderZeroOffset = 1234;
            d.encoderDirection = 1;
            d.polePairs = 7;
            d.kpCurrent = 10.0f;
            d.kiCurrent = 500.0f;
            d.kpVelocity = 0.5f;
            d.kiVelocity = 20.0f;
            return d;
        }

        services::ConfigData MakeTestConfig()
        {
            services::ConfigData c{};
            c.defaultControlMode = 1;
            c.maxCurrent = 10.0f;
            c.maxVelocity = 3000.0f;
            c.maxTorque = 2.5f;
            c.canNodeId = 5;
            c.canBaudrate = 1000000;
            c.telemetryRateHz = 200;
            c.overTempThreshold = 90.0f;
            c.underVoltageThreshold = 12.0f;
            c.overVoltageThreshold = 30.0f;
            return c;
        }

        void RunUntilDone(bool& done)
        {
            while (!done)
                ExecuteAllActions();
        }
    };
}

TEST_F(NonVolatileMemoryTest, save_calibration_completes_with_ok)
{
    bool done = false;
    services::NvmStatus result{};

    nvm.SaveCalibration(MakeTestCalibration(), [&](services::NvmStatus s)
        {
            result = s;
            done = true;
        });

    RunUntilDone(done);
    EXPECT_EQ(result, services::NvmStatus::Ok);
}

TEST_F(NonVolatileMemoryTest, load_calibration_returns_ok_with_correct_data_after_save)
{
    const auto written = MakeTestCalibration();
    bool saveDone = false;
    nvm.SaveCalibration(written, [&](auto)
        {
            saveDone = true;
        });
    RunUntilDone(saveDone);

    services::CalibrationData loaded{};
    bool loadDone = false;
    services::NvmStatus result{};
    nvm.LoadCalibration(loaded, [&](services::NvmStatus s)
        {
            result = s;
            loadDone = true;
        });

    RunUntilDone(loadDone);
    EXPECT_EQ(result, services::NvmStatus::Ok);
    EXPECT_EQ(std::memcmp(&loaded, &written, sizeof(services::CalibrationData)), 0);
}

TEST_F(NonVolatileMemoryTest, load_calibration_returns_invalid_data_on_blank_flash)
{
    services::CalibrationData out{};
    bool done = false;
    services::NvmStatus result{};

    nvm.LoadCalibration(out, [&](services::NvmStatus s)
        {
            result = s;
            done = true;
        });

    RunUntilDone(done);
    EXPECT_EQ(result, services::NvmStatus::InvalidData);
}

TEST_F(NonVolatileMemoryTest, load_calibration_returns_version_mismatch_on_wrong_version)
{
    services::CalibrationStorageRecord record{};
    record.magic = services::CalibrationMagic;
    record.version = services::CalibrationLayoutVersion + 1;
    record.data = MakeTestCalibration();
    infra::Crc32 crc;
    crc.Update(infra::MakeConstByteRange(record.data));
    record.crc32 = crc.Result();

    auto bytes = infra::MakeConstByteRange(record);
    std::copy(bytes.begin(), bytes.end(), flash.sectors[0].begin());

    services::CalibrationData out{};
    bool done = false;
    services::NvmStatus result{};

    nvm.LoadCalibration(out, [&](services::NvmStatus s)
        {
            result = s;
            done = true;
        });

    RunUntilDone(done);
    EXPECT_EQ(result, services::NvmStatus::VersionMismatch);
}

TEST_F(NonVolatileMemoryTest, load_calibration_returns_invalid_data_on_crc_corruption)
{
    bool saveDone = false;
    nvm.SaveCalibration(MakeTestCalibration(), [&](auto)
        {
            saveDone = true;
        });
    RunUntilDone(saveDone);

    constexpr std::size_t crcOffset = offsetof(services::CalibrationStorageRecord, crc32);
    flash.sectors[0][crcOffset] ^= 0xFF;

    services::CalibrationData out{};
    bool done = false;
    services::NvmStatus result{};

    nvm.LoadCalibration(out, [&](services::NvmStatus s)
        {
            result = s;
            done = true;
        });

    RunUntilDone(done);
    EXPECT_EQ(result, services::NvmStatus::InvalidData);
}

TEST_F(NonVolatileMemoryTest, is_calibration_valid_returns_true_after_save)
{
    bool saveDone = false;
    nvm.SaveCalibration(MakeTestCalibration(), [&](auto)
        {
            saveDone = true;
        });
    RunUntilDone(saveDone);

    bool done = false;
    bool valid = false;
    nvm.IsCalibrationValid([&](bool v)
        {
            valid = v;
            done = true;
        });
    RunUntilDone(done);

    EXPECT_TRUE(valid);
}

TEST_F(NonVolatileMemoryTest, is_calibration_valid_returns_false_on_blank_flash)
{
    bool done = false;
    bool valid = true;
    nvm.IsCalibrationValid([&](bool v)
        {
            valid = v;
            done = true;
        });
    RunUntilDone(done);

    EXPECT_FALSE(valid);
}

TEST_F(NonVolatileMemoryTest, invalidate_calibration_makes_is_calibration_valid_return_false)
{
    bool saveDone = false;
    nvm.SaveCalibration(MakeTestCalibration(), [&](auto)
        {
            saveDone = true;
        });
    RunUntilDone(saveDone);

    bool invalidateDone = false;
    nvm.InvalidateCalibration([&](auto)
        {
            invalidateDone = true;
        });
    RunUntilDone(invalidateDone);

    bool done = false;
    bool valid = true;
    nvm.IsCalibrationValid([&](bool v)
        {
            valid = v;
            done = true;
        });
    RunUntilDone(done);

    EXPECT_FALSE(valid);
}

TEST_F(NonVolatileMemoryTest, save_config_completes_with_ok)
{
    bool done = false;
    services::NvmStatus result{};

    nvm.SaveConfig(MakeTestConfig(), [&](services::NvmStatus s)
        {
            result = s;
            done = true;
        });

    RunUntilDone(done);
    EXPECT_EQ(result, services::NvmStatus::Ok);
}

TEST_F(NonVolatileMemoryTest, load_config_returns_ok_with_correct_data_after_save)
{
    const auto written = MakeTestConfig();
    bool saveDone = false;
    nvm.SaveConfig(written, [&](auto)
        {
            saveDone = true;
        });
    RunUntilDone(saveDone);

    services::ConfigData loaded{};
    bool done = false;
    services::NvmStatus result{};

    nvm.LoadConfig(loaded, [&](services::NvmStatus s)
        {
            result = s;
            done = true;
        });

    RunUntilDone(done);
    EXPECT_EQ(result, services::NvmStatus::Ok);
    EXPECT_EQ(std::memcmp(&loaded, &written, sizeof(services::ConfigData)), 0);
}

TEST_F(NonVolatileMemoryTest, load_config_returns_defaults_when_flash_is_blank)
{
    const services::ConfigData defaults = services::MakeDefaultConfigData();
    services::ConfigData loaded{};
    bool done = false;
    services::NvmStatus result{};

    nvm.LoadConfig(loaded, [&](services::NvmStatus s)
        {
            result = s;
            done = true;
        });

    RunUntilDone(done);
    EXPECT_EQ(result, services::NvmStatus::Ok);
    EXPECT_EQ(std::memcmp(&loaded, &defaults, sizeof(services::ConfigData)), 0);
}

TEST_F(NonVolatileMemoryTest, reset_config_to_defaults_stores_default_values)
{
    bool saveDone = false;
    nvm.SaveConfig(MakeTestConfig(), [&](auto)
        {
            saveDone = true;
        });
    RunUntilDone(saveDone);

    bool resetDone = false;
    nvm.ResetConfigToDefaults([&](auto)
        {
            resetDone = true;
        });
    RunUntilDone(resetDone);

    const services::ConfigData defaults = services::MakeDefaultConfigData();
    services::ConfigData loaded{};
    bool done = false;

    nvm.LoadConfig(loaded, [&](auto)
        {
            done = true;
        });
    RunUntilDone(done);

    EXPECT_EQ(std::memcmp(&loaded, &defaults, sizeof(services::ConfigData)), 0);
}

TEST_F(NonVolatileMemoryTest, format_erases_all_regions)
{
    bool s1 = false;
    bool s2 = false;
    nvm.SaveCalibration(MakeTestCalibration(), [&](auto)
        {
            s1 = true;
        });
    RunUntilDone(s1);
    nvm.SaveConfig(MakeTestConfig(), [&](auto)
        {
            s2 = true;
        });
    RunUntilDone(s2);

    bool done = false;
    nvm.Format([&](services::NvmStatus s)
        {
            EXPECT_EQ(s, services::NvmStatus::Ok);
            done = true;
        });
    RunUntilDone(done);

    for (auto byte : flash.sectors[0])
        EXPECT_EQ(byte, 0xFF);
    for (auto byte : flash.sectors[1])
        EXPECT_EQ(byte, 0xFF);
}
