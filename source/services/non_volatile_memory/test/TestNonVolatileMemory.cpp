#include "infra/event/EventDispatcher.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/Crc.hpp"
#include "source/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include <algorithm>
#include <cstring>
#include <gmock/gmock.h>
#include <vector>

namespace
{
    using namespace testing;

    // ---------------------------------------------------------------------------
    // NvmRegionStub — in-memory region that dispatches callbacks via the event loop,
    // matching the async behaviour of real flash and EEPROM implementations.
    // ---------------------------------------------------------------------------
    class NvmRegionStub
        : public services::NvmRegion
    {
    public:
        explicit NvmRegionStub(std::size_t size)
            : storage(size, 0xFF)
        {}

        void Write(infra::ConstByteRange data, infra::Function<void()> onDone) override
        {
            std::copy(data.begin(), data.end(), storage.begin());
            infra::EventDispatcher::Instance().Schedule(onDone);
        }

        void Read(infra::ByteRange data, infra::Function<void()> onDone) override
        {
            std::copy(storage.begin(), storage.begin() + data.size(), data.begin());
            infra::EventDispatcher::Instance().Schedule(onDone);
        }

        void Erase(infra::Function<void()> onDone) override
        {
            std::fill(storage.begin(), storage.end(), 0xFF);
            infra::EventDispatcher::Instance().Schedule(onDone);
        }

        std::size_t Size() const override
        {
            return storage.size();
        }

        std::vector<uint8_t> storage;
    };

    // ---------------------------------------------------------------------------
    // Helper: build a calibration record directly into a region's storage buffer.
    // Record layout: [magic:4][version:1][crc32:4][data:sizeof(CalibrationData)]
    // ---------------------------------------------------------------------------
    static constexpr std::size_t recordMagicOffset = 0;
    static constexpr std::size_t recordVersionOffset = 4;
    static constexpr std::size_t recordCrc32Offset = 5;
    static constexpr std::size_t recordDataOffset = 9;

    void WriteCalibrationRecord(NvmRegionStub& region, uint32_t magic, uint8_t version,
        const services::CalibrationData& data)
    {
        std::memcpy(region.storage.data() + recordMagicOffset, &magic, sizeof(magic));
        std::memcpy(region.storage.data() + recordVersionOffset, &version, sizeof(version));
        std::memcpy(region.storage.data() + recordDataOffset, &data, sizeof(data));
        infra::Crc32 crc;
        crc.Update(infra::ConstByteRange(
            region.storage.data() + recordDataOffset,
            region.storage.data() + recordDataOffset + sizeof(data)));
        uint32_t crcValue = crc.Result();
        std::memcpy(region.storage.data() + recordCrc32Offset, &crcValue, sizeof(crcValue));
    }

    // ---------------------------------------------------------------------------
    // Field-by-field equality helpers — avoids relying on memcmp over padding bytes.
    // ---------------------------------------------------------------------------
    void ExpectCalibrationDataEqual(const services::CalibrationData& actual,
        const services::CalibrationData& expected)
    {
        EXPECT_EQ(actual.rPhase, expected.rPhase);
        EXPECT_EQ(actual.lD, expected.lD);
        EXPECT_EQ(actual.lQ, expected.lQ);
        EXPECT_EQ(actual.currentOffsetA, expected.currentOffsetA);
        EXPECT_EQ(actual.currentOffsetB, expected.currentOffsetB);
        EXPECT_EQ(actual.currentOffsetC, expected.currentOffsetC);
        EXPECT_EQ(actual.inertia, expected.inertia);
        EXPECT_EQ(actual.frictionCoulomb, expected.frictionCoulomb);
        EXPECT_EQ(actual.frictionViscous, expected.frictionViscous);
        EXPECT_EQ(actual.encoderZeroOffset, expected.encoderZeroOffset);
        EXPECT_EQ(actual.kpCurrent, expected.kpCurrent);
        EXPECT_EQ(actual.kiCurrent, expected.kiCurrent);
        EXPECT_EQ(actual.kpVelocity, expected.kpVelocity);
        EXPECT_EQ(actual.kiVelocity, expected.kiVelocity);
        EXPECT_EQ(actual.encoderDirection, expected.encoderDirection);
        EXPECT_EQ(actual.polePairs, expected.polePairs);
    }

    void ExpectConfigDataEqual(const services::ConfigData& actual,
        const services::ConfigData& expected)
    {
        EXPECT_EQ(actual.maxCurrent, expected.maxCurrent);
        EXPECT_EQ(actual.maxVelocity, expected.maxVelocity);
        EXPECT_EQ(actual.maxTorque, expected.maxTorque);
        EXPECT_EQ(actual.canNodeId, expected.canNodeId);
        EXPECT_EQ(actual.canBaudrate, expected.canBaudrate);
        EXPECT_EQ(actual.telemetryRateHz, expected.telemetryRateHz);
        EXPECT_EQ(actual.overTempThreshold, expected.overTempThreshold);
        EXPECT_EQ(actual.underVoltageThreshold, expected.underVoltageThreshold);
        EXPECT_EQ(actual.overVoltageThreshold, expected.overVoltageThreshold);
        EXPECT_EQ(actual.defaultControlMode, expected.defaultControlMode);
    }

    // ---------------------------------------------------------------------------
    // Fixture
    // ---------------------------------------------------------------------------
    class NonVolatileMemoryTest
        : public ::testing::Test
        , public infra::EventDispatcherFixture
    {
    protected:
        NvmRegionStub calibrationRegion{ services::NonVolatileMemoryImpl::calibrationRecordSize };
        NvmRegionStub configRegion{ services::NonVolatileMemoryImpl::configRecordSize };
        services::NonVolatileMemoryImpl nvm{ calibrationRegion, configRegion };

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
            constexpr int maxIterations = 1000;
            for (int i = 0; i < maxIterations; ++i)
            {
                if (done)
                    return;
                ExecuteAllActions();
            }
            FAIL() << "Async callback was not invoked within " << maxIterations << " iterations";
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
    ExpectCalibrationDataEqual(loaded, written);
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
    WriteCalibrationRecord(calibrationRegion, services::CalibrationMagic,
        services::CalibrationLayoutVersion + 1, MakeTestCalibration());

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

    // Corrupt one byte of the CRC field (record layout: [magic:4][version:1][crc32:4]...)
    calibrationRegion.storage[recordCrc32Offset] ^= 0xFF;

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
    ExpectConfigDataEqual(loaded, written);
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
    ExpectConfigDataEqual(loaded, defaults);
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

    ExpectConfigDataEqual(loaded, defaults);
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

    for (auto byte : calibrationRegion.storage)
        EXPECT_EQ(byte, 0xFF);
    for (auto byte : configRegion.storage)
        EXPECT_EQ(byte, 0xFF);
}
