#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/Crc.hpp"
#include <algorithm>
#include <cstring>
#include <gmock/gmock.h>
#include <optional>
#include <vector>

namespace
{
    using namespace testing;

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

    // Record layout: [magic:4][version:1][crc32:4][data:sizeof(CalibrationData)]
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

    void ExpectCalibrationDataEqual(const services::CalibrationData& actual,
        const services::CalibrationData& expected)
    {
        EXPECT_EQ(actual.rPhase, expected.rPhase);
        EXPECT_EQ(actual.lD, expected.lD);
        EXPECT_EQ(actual.lQ, expected.lQ);
        EXPECT_EQ(actual.fluxLinkage, expected.fluxLinkage);
        EXPECT_EQ(actual.currentOffsetA, expected.currentOffsetA);
        EXPECT_EQ(actual.currentOffsetB, expected.currentOffsetB);
        EXPECT_EQ(actual.currentOffsetC, expected.currentOffsetC);
        EXPECT_EQ(actual.inertia, expected.inertia);
        EXPECT_EQ(actual.frictionCoulomb, expected.frictionCoulomb);
        EXPECT_EQ(actual.frictionViscous, expected.frictionViscous);
        EXPECT_EQ(actual.encoderZeroOffset, expected.encoderZeroOffset);
        EXPECT_EQ(actual.currentLoopBandwidth, expected.currentLoopBandwidth);
        EXPECT_EQ(actual.speedLoopBandwidth, expected.speedLoopBandwidth);
        EXPECT_EQ(actual.encoderDirection, expected.encoderDirection);
        EXPECT_EQ(actual.polePairs, expected.polePairs);
    }

    void ExpectConfigDataEqual(const services::ConfigData& actual,
        const services::ConfigData& expected)
    {
        EXPECT_EQ(actual.canNodeId, expected.canNodeId);
        EXPECT_EQ(actual.canBaudrate, expected.canBaudrate);
        EXPECT_EQ(actual.telemetryRateHz, expected.telemetryRateHz);
        EXPECT_EQ(actual.encoderResolution, expected.encoderResolution);
        EXPECT_EQ(actual.defaultControlMode, expected.defaultControlMode);
    }

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
            d.fluxLinkage = 0.007f;
            d.currentOffsetA = 0.01f;
            d.currentOffsetB = -0.01f;
            d.currentOffsetC = 0.005f;
            d.inertia = 0.0002f;
            d.frictionCoulomb = 0.05f;
            d.frictionViscous = 0.001f;
            d.encoderZeroOffset = 1234;
            d.encoderDirection = 1;
            d.polePairs = 7;
            d.currentLoopBandwidth = 8377.6f;
            d.speedLoopBandwidth = 50.0f;
            return d;
        }

        services::ConfigData MakeTestConfig()
        {
            services::ConfigData c{};
            c.defaultControlMode = 1;
            c.canNodeId = 5;
            c.canBaudrate = 1000000;
            c.telemetryRateHz = 200;
            c.encoderResolution = 8000;
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

    class NvmRegionMock
        : public services::NvmRegion
    {
    public:
        MOCK_METHOD(void, Write, (infra::ConstByteRange data, infra::Function<void()> onDone), (override));
        MOCK_METHOD(void, Read, (infra::ByteRange data, infra::Function<void()> onDone), (override));
        MOCK_METHOD(void, Erase, (infra::Function<void()> onDone), (override));
        MOCK_METHOD(std::size_t, Size, (), (const, override));
    };

    class NonVolatileMemorySingleWriterTest
        : public ::testing::Test
    {
    protected:
        void StartConfigWrite()
        {
            EXPECT_CALL(configRegion, Erase(_)).WillOnce(SaveArg<0>(&pendingRegionCallback));
            nvm.SaveConfig(services::ConfigData{}, [this](services::NvmStatus status)
                {
                    configStatus = status;
                });
        }

        void StartCalibrationWrite()
        {
            EXPECT_CALL(calibrationRegion, Erase(_)).WillOnce(SaveArg<0>(&pendingRegionCallback));
            nvm.SaveCalibration(services::CalibrationData{}, [this](services::NvmStatus status)
                {
                    calibrationStatus = status;
                });
        }

        void InvokePendingRegionCallback()
        {
            ASSERT_TRUE(pendingRegionCallback != nullptr);
            auto callback = pendingRegionCallback;
            pendingRegionCallback = nullptr;
            callback();
        }

        void FinishWrite(testing::StrictMock<NvmRegionMock>& region)
        {
            EXPECT_CALL(region, Write(_, _)).WillOnce(SaveArg<1>(&pendingRegionCallback));
            InvokePendingRegionCallback();
            EXPECT_CALL(region, Read(_, _)).WillOnce(SaveArg<1>(&pendingRegionCallback));
            InvokePendingRegionCallback();
            InvokePendingRegionCallback();
        }

        testing::StrictMock<NvmRegionMock> calibrationRegion;
        testing::StrictMock<NvmRegionMock> configRegion;
        services::NonVolatileMemoryImpl nvm{ calibrationRegion, configRegion };

        infra::Function<void()> pendingRegionCallback;
        std::optional<services::NvmStatus> configStatus;
        std::optional<services::NvmStatus> calibrationStatus;
        std::optional<bool> calibrationValid;
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

TEST_F(NonVolatileMemoryTest, load_calibration_returns_version_mismatch_on_previous_layout)
{
    WriteCalibrationRecord(calibrationRegion, services::CalibrationMagic,
        services::CalibrationLayoutVersion - 1, MakeTestCalibration());

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
    EXPECT_EQ(out.rPhase, 0.0f);
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

TEST_F(NonVolatileMemoryTest, load_config_returns_defaults_on_crc_corruption)
{
    bool saveDone = false;
    nvm.SaveConfig(MakeTestConfig(), [&](auto)
        {
            saveDone = true;
        });
    RunUntilDone(saveDone);

    // Corrupt one byte of the CRC field (same offset as calibration record)
    configRegion.storage[recordCrc32Offset] ^= 0xFF;

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

TEST_F(NonVolatileMemoryTest, save_calibration_write_verify_failure_returns_write_failed)
{
    // Override Read to return corrupted data so the readback comparison fails.
    struct CorruptingRegion
        : public services::NvmRegion
    {
        std::vector<uint8_t> storage;
        bool corruptOnRead{ false };

        explicit CorruptingRegion(std::size_t size)
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
            if (corruptOnRead && !data.empty())
                data[0] ^= 0xFF;
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
    };

    CorruptingRegion corruptCalibrationRegion{ services::NonVolatileMemoryImpl::calibrationRecordSize };
    NvmRegionStub corruptConfigRegion{ services::NonVolatileMemoryImpl::configRecordSize };
    services::NonVolatileMemoryImpl nvmWithCorruption{ corruptCalibrationRegion, corruptConfigRegion };

    corruptCalibrationRegion.corruptOnRead = true;

    bool done = false;
    services::NvmStatus result{};

    nvmWithCorruption.SaveCalibration(MakeTestCalibration(), [&](services::NvmStatus s)
        {
            result = s;
            done = true;
        });

    RunUntilDone(done);
    EXPECT_EQ(result, services::NvmStatus::WriteFailed);
}

TEST_F(NonVolatileMemoryTest, save_config_write_verify_failure_returns_write_failed)
{
    struct CorruptingRegion
        : public services::NvmRegion
    {
        std::vector<uint8_t> storage;

        explicit CorruptingRegion(std::size_t size)
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
            if (!data.empty())
                data[0] ^= 0xFF;
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
    };

    NvmRegionStub normalCalibrationRegion{ services::NonVolatileMemoryImpl::calibrationRecordSize };
    CorruptingRegion corruptConfigRegion{ services::NonVolatileMemoryImpl::configRecordSize };
    services::NonVolatileMemoryImpl nvmWithCorruption{ normalCalibrationRegion, corruptConfigRegion };

    bool done = false;
    services::NvmStatus result{};

    nvmWithCorruption.SaveConfig(MakeTestConfig(), [&](services::NvmStatus s)
        {
            result = s;
            done = true;
        });

    RunUntilDone(done);
    EXPECT_EQ(result, services::NvmStatus::WriteFailed);
}

TEST_F(NonVolatileMemoryTest, concurrent_save_calibration_second_call_is_rejected)
{
    bool firstDone = false;

    nvm.SaveCalibration(MakeTestCalibration(), [&](auto)
        {
            firstDone = true;
        });

    std::optional<services::NvmStatus> secondStatus;
    nvm.SaveCalibration(MakeTestCalibration(), [&](services::NvmStatus s)
        {
            secondStatus = s;
        });

    ASSERT_TRUE(secondStatus.has_value());
    EXPECT_EQ(*secondStatus, services::NvmStatus::Busy);

    RunUntilDone(firstDone);

    EXPECT_TRUE(firstDone);
}

TEST_F(NonVolatileMemoryTest, concurrent_load_config_second_call_is_rejected)
{
    bool saveDone = false;
    nvm.SaveConfig(MakeTestConfig(), [&](auto)
        {
            saveDone = true;
        });
    RunUntilDone(saveDone);

    services::ConfigData out1{};
    services::ConfigData out2{};

    bool firstDone = false;

    nvm.LoadConfig(out1, [&](auto)
        {
            firstDone = true;
        });

    std::optional<services::NvmStatus> secondStatus;
    nvm.LoadConfig(out2, [&](services::NvmStatus s)
        {
            secondStatus = s;
        });

    ASSERT_TRUE(secondStatus.has_value());
    EXPECT_EQ(*secondStatus, services::NvmStatus::Busy);

    RunUntilDone(firstDone);

    EXPECT_TRUE(firstDone);
}

TEST_F(NonVolatileMemorySingleWriterTest, save_calibration_during_config_write_reports_busy_without_touching_the_region)
{
    StartConfigWrite();

    nvm.SaveCalibration(services::CalibrationData{}, [this](services::NvmStatus status)
        {
            calibrationStatus = status;
        });

    ASSERT_TRUE(calibrationStatus.has_value());
    EXPECT_EQ(*calibrationStatus, services::NvmStatus::Busy);
}

TEST_F(NonVolatileMemorySingleWriterTest, invalidate_calibration_during_config_write_reports_busy_without_touching_the_region)
{
    StartConfigWrite();

    nvm.InvalidateCalibration([this](services::NvmStatus status)
        {
            calibrationStatus = status;
        });

    ASSERT_TRUE(calibrationStatus.has_value());
    EXPECT_EQ(*calibrationStatus, services::NvmStatus::Busy);
}

TEST_F(NonVolatileMemorySingleWriterTest, is_calibration_valid_during_config_write_reports_false_without_touching_the_region)
{
    StartConfigWrite();

    nvm.IsCalibrationValid([this](bool valid)
        {
            calibrationValid = valid;
        });

    ASSERT_TRUE(calibrationValid.has_value());
    EXPECT_FALSE(*calibrationValid);
}

TEST_F(NonVolatileMemorySingleWriterTest, save_config_during_calibration_write_reports_busy_without_touching_the_region)
{
    StartCalibrationWrite();

    nvm.SaveConfig(services::ConfigData{}, [this](services::NvmStatus status)
        {
            configStatus = status;
        });

    ASSERT_TRUE(configStatus.has_value());
    EXPECT_EQ(*configStatus, services::NvmStatus::Busy);
}

TEST_F(NonVolatileMemorySingleWriterTest, format_during_config_write_reports_busy_without_touching_the_region)
{
    StartConfigWrite();

    std::optional<services::NvmStatus> formatStatus;
    nvm.Format([&formatStatus](services::NvmStatus status)
        {
            formatStatus = status;
        });

    ASSERT_TRUE(formatStatus.has_value());
    EXPECT_EQ(*formatStatus, services::NvmStatus::Busy);
}

TEST_F(NonVolatileMemorySingleWriterTest, calibration_request_is_accepted_after_the_config_write_completes)
{
    StartConfigWrite();

    nvm.InvalidateCalibration([this](services::NvmStatus status)
        {
            calibrationStatus = status;
        });
    ASSERT_EQ(*calibrationStatus, services::NvmStatus::Busy);
    calibrationStatus.reset();

    FinishWrite(configRegion);
    ASSERT_TRUE(configStatus.has_value());

    EXPECT_CALL(calibrationRegion, Erase(_)).WillOnce(SaveArg<0>(&pendingRegionCallback));
    EXPECT_CALL(calibrationRegion, Read(_, _)).WillOnce(SaveArg<1>(&pendingRegionCallback));
    nvm.InvalidateCalibration([this](services::NvmStatus status)
        {
            calibrationStatus = status;
        });
    EXPECT_FALSE(calibrationStatus.has_value());

    InvokePendingRegionCallback();
    EXPECT_FALSE(calibrationStatus.has_value());

    InvokePendingRegionCallback();

    ASSERT_TRUE(calibrationStatus.has_value());
    EXPECT_EQ(*calibrationStatus, services::NvmStatus::Ok);
}
