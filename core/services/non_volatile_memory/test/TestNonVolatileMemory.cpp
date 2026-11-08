#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include <algorithm>
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

        std::vector<uint8_t> storage;
    };

    static constexpr std::size_t recordMagicOffset = 0;
    static constexpr std::size_t recordVersionOffset = services::CalibrationRecord::versionOffset;
    static constexpr std::size_t recordCrc32Offset = services::CalibrationRecord::crcOffset;
    static constexpr std::size_t recordDataOffset = services::CalibrationRecord::dataOffset;

    void WriteCalibrationRecordOfVersion(NvmRegionStub& region, uint8_t version, const services::CalibrationData& data)
    {
        services::CalibrationRecord::Serialize(data, infra::ByteRange{ region.storage.data(), region.storage.data() + services::CalibrationRecord::size });
        region.storage[recordVersionOffset] = version;
    }

    void ExpectCalibrationDataEqual(const services::CalibrationData& actual,
        const services::CalibrationData& expected)
    {
        EXPECT_EQ(actual.rPhase, expected.rPhase);
        EXPECT_EQ(actual.lD, expected.lD);
        EXPECT_EQ(actual.fluxLinkage, expected.fluxLinkage);
        EXPECT_EQ(actual.currentOffsetA, expected.currentOffsetA);
        EXPECT_EQ(actual.currentOffsetB, expected.currentOffsetB);
        EXPECT_EQ(actual.currentOffsetC, expected.currentOffsetC);
        EXPECT_EQ(actual.inertia, expected.inertia);
        EXPECT_EQ(actual.frictionCoulomb, expected.frictionCoulomb);
        EXPECT_EQ(actual.frictionViscous, expected.frictionViscous);
        EXPECT_EQ(actual.speedLoopBandwidth, expected.speedLoopBandwidth);
        EXPECT_EQ(actual.coggingTorqueScale, expected.coggingTorqueScale);
        EXPECT_EQ(services::EncoderIndexAngle(actual), services::EncoderIndexAngle(expected));
        EXPECT_EQ(actual.currentNoiseMilliAmpere, expected.currentNoiseMilliAmpere);
        EXPECT_EQ(actual.polePairs, expected.polePairs);
        EXPECT_EQ(actual.stage, expected.stage);
        EXPECT_EQ(actual.coggingTorque, expected.coggingTorque);
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
        NvmRegionStub calibrationRegion{ services::CalibrationRecord::size };
        NvmRegionStub configRegion{ services::ConfigRecord::size };
        NvmRegionStub faultHistoryRegion{ services::faultHistoryImageSize };
        NvmRegionStub learnedModelRegion{ services::learnedModelImageSize };
        services::NonVolatileMemoryImpl nvm{ calibrationRegion, configRegion, faultHistoryRegion, learnedModelRegion };

        services::CalibrationData MakeTestCalibration()
        {
            services::CalibrationData d{};
            d.rPhase = 1.5f;
            d.lD = 0.001f;
            d.fluxLinkage = 0.007f;
            d.currentOffsetA = 0.01f;
            d.currentOffsetB = -0.01f;
            d.currentOffsetC = 0.005f;
            d.inertia = 0.0002f;
            d.frictionCoulomb = 0.05f;
            d.frictionViscous = 0.001f;
            d.polePairs = 7;
            d.stage = services::CalibrationStage::complete;
            d.currentNoiseMilliAmpere = 300;
            d.speedLoopBandwidth = 50.0f;
            d.coggingTorqueScale = 3.0e-5f;
            d.encoderIndexAngle = -2.25f;
            for (std::size_t bin = 0; bin != d.coggingTorque.size(); ++bin)
                d.coggingTorque[bin] = static_cast<int8_t>(static_cast<int>(bin * 5) % 255 - 127);
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
    };

    class NonVolatileMemorySingleWriterTest
        : public ::testing::Test
    {
    protected:
        void StartConfigWrite()
        {
            EXPECT_CALL(configRegion, Write(_, _)).WillOnce(SaveArg<1>(&pendingRegionCallback));
            nvm.SaveConfig(services::ConfigData{}, [this](services::NvmStatus status)
                {
                    configStatus = status;
                });
        }

        void StartCalibrationWrite()
        {
            EXPECT_CALL(calibrationRegion, Write(_, _)).WillOnce(SaveArg<1>(&pendingRegionCallback));
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
            EXPECT_CALL(region, Read(_, _)).WillOnce(SaveArg<1>(&pendingRegionCallback));
            InvokePendingRegionCallback();
            InvokePendingRegionCallback();
        }

        testing::StrictMock<NvmRegionMock> calibrationRegion;
        testing::StrictMock<NvmRegionMock> configRegion;
        testing::StrictMock<NvmRegionMock> faultHistoryRegion;
        testing::StrictMock<NvmRegionMock> learnedModelRegion;
        services::NonVolatileMemoryImpl nvm{ calibrationRegion, configRegion, faultHistoryRegion, learnedModelRegion };

        infra::Function<void()> pendingRegionCallback;
        std::optional<services::NvmStatus> configStatus;
        std::optional<services::NvmStatus> calibrationStatus;
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

TEST_F(NonVolatileMemoryTest, load_calibration_returns_invalid_data_on_another_version)
{
    WriteCalibrationRecordOfVersion(calibrationRegion, services::CalibrationLayoutVersion + 1, MakeTestCalibration());

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

TEST_F(NonVolatileMemoryTest, load_calibration_returns_invalid_data_on_the_previous_layout)
{
    WriteCalibrationRecordOfVersion(calibrationRegion, services::CalibrationLayoutVersion - 1, MakeTestCalibration());

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

TEST_F(NonVolatileMemoryTest, load_calibration_on_magic_mismatch_zero_initialises_the_output)
{
    // Blank flash leaves the magic field at 0xFFFFFFFF, which does not match CalibrationMagic.
    std::fill(calibrationRegion.storage.begin(), calibrationRegion.storage.end(), 0xFF);

    services::CalibrationData out = MakeTestCalibration();
    bool done = false;
    services::NvmStatus result{};

    nvm.LoadCalibration(out, [&](services::NvmStatus s)
        {
            result = s;
            done = true;
        });

    RunUntilDone(done);
    EXPECT_EQ(result, services::NvmStatus::InvalidData);
    ExpectCalibrationDataEqual(out, services::CalibrationData{});
}

TEST_F(NonVolatileMemoryTest, load_calibration_on_crc_mismatch_zero_initialises_the_output)
{
    bool saveDone = false;
    nvm.SaveCalibration(MakeTestCalibration(), [&](auto)
        {
            saveDone = true;
        });
    RunUntilDone(saveDone);

    // Corrupt the data so the stored CRC no longer matches while magic and version stay valid.
    calibrationRegion.storage[recordDataOffset] ^= 0xFF;

    services::CalibrationData out = MakeTestCalibration();
    bool done = false;
    services::NvmStatus result{};

    nvm.LoadCalibration(out, [&](services::NvmStatus s)
        {
            result = s;
            done = true;
        });

    RunUntilDone(done);
    EXPECT_EQ(result, services::NvmStatus::InvalidData);
    ExpectCalibrationDataEqual(out, services::CalibrationData{});
}

TEST_F(NonVolatileMemoryTest, invalidate_calibration_makes_the_next_load_invalid)
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

TEST_F(NonVolatileMemoryTest, an_invalidation_whose_erase_leaves_the_record_intact_reports_write_failed)
{
    struct EraseIgnoringRegion
        : NvmRegionStub
    {
        using NvmRegionStub::NvmRegionStub;

        void Erase(infra::Function<void()> onDone) override
        {
            infra::EventDispatcher::Instance().Schedule(onDone);
        }
    };

    EraseIgnoringRegion stuckCalibrationRegion{ services::CalibrationRecord::size };
    services::NonVolatileMemoryImpl nvmWithStuckRegion{ stuckCalibrationRegion, configRegion, faultHistoryRegion, learnedModelRegion };
    nvmWithStuckRegion.SaveCalibration(MakeTestCalibration(), [](services::NvmStatus) {});
    ExecuteAllActions();

    std::optional<services::NvmStatus> status;
    nvmWithStuckRegion.InvalidateCalibration([&status](services::NvmStatus result)
        {
            status = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(services::NvmStatus::WriteFailed, status);
}

TEST_F(NonVolatileMemoryTest, a_save_writes_the_record_without_erasing_the_region_first)
{
    struct EraseCountingRegion
        : NvmRegionStub
    {
        using NvmRegionStub::NvmRegionStub;

        void Erase(infra::Function<void()> onDone) override
        {
            ++erases;
            NvmRegionStub::Erase(onDone);
        }

        int erases{ 0 };
    };

    EraseCountingRegion countingCalibrationRegion{ services::CalibrationRecord::size };
    EraseCountingRegion countingConfigRegion{ services::ConfigRecord::size };
    services::NonVolatileMemoryImpl nvmCounting{ countingCalibrationRegion, countingConfigRegion, faultHistoryRegion, learnedModelRegion };

    nvmCounting.SaveCalibration(MakeTestCalibration(), [](services::NvmStatus) {});
    ExecuteAllActions();
    nvmCounting.SaveConfig(MakeTestConfig(), [](services::NvmStatus) {});
    ExecuteAllActions();

    EXPECT_EQ(0, countingCalibrationRegion.erases);
    EXPECT_EQ(0, countingConfigRegion.erases);
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

TEST_F(NonVolatileMemoryTest, the_homing_requirement_survives_a_save_and_a_load)
{
    auto written = MakeTestConfig();
    written.homingRequired = 1;
    bool saveDone = false;
    nvm.SaveConfig(written, [&](auto)
        {
            saveDone = true;
        });
    RunUntilDone(saveDone);

    services::ConfigData loaded{};
    bool done = false;
    nvm.LoadConfig(loaded, [&](auto)
        {
            done = true;
        });
    RunUntilDone(done);

    EXPECT_EQ(loaded.homingRequired, uint8_t{ 1 });
}

TEST_F(NonVolatileMemoryTest, the_config_layout_is_version_14)
{
    EXPECT_EQ(services::ConfigLayoutVersion, 14);
}

TEST_F(NonVolatileMemoryTest, load_config_returns_defaults_when_flash_is_blank)
{
    const services::ConfigData defaults = services::ConfigData{};
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

TEST_F(NonVolatileMemoryTest, load_config_returns_defaults_on_crc_corruption)
{
    bool saveDone = false;
    nvm.SaveConfig(MakeTestConfig(), [&](auto)
        {
            saveDone = true;
        });
    RunUntilDone(saveDone);

    configRegion.storage[recordCrc32Offset] ^= 0xFF;

    const services::ConfigData defaults = services::ConfigData{};
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

TEST_F(NonVolatileMemoryTest, load_config_returns_defaults_on_magic_mismatch)
{
    bool saveDone = false;
    nvm.SaveConfig(MakeTestConfig(), [&](auto)
        {
            saveDone = true;
        });
    RunUntilDone(saveDone);

    configRegion.storage[recordMagicOffset] ^= 0xFF;

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
    ExpectConfigDataEqual(loaded, services::ConfigData{});
}

TEST_F(NonVolatileMemoryTest, load_config_returns_defaults_on_stale_version)
{
    bool saveDone = false;
    nvm.SaveConfig(MakeTestConfig(), [&](auto)
        {
            saveDone = true;
        });
    RunUntilDone(saveDone);

    configRegion.storage[recordVersionOffset] = services::ConfigLayoutVersion - 1;

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
    ExpectConfigDataEqual(loaded, services::ConfigData{});
}

TEST_F(NonVolatileMemoryTest, save_calibration_write_verify_failure_returns_hardware_fault)
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
    };

    CorruptingRegion corruptCalibrationRegion{ services::CalibrationRecord::size };
    NvmRegionStub corruptConfigRegion{ services::ConfigRecord::size };
    NvmRegionStub faultHistoryRegionStub{ services::faultHistoryImageSize };
    services::NonVolatileMemoryImpl nvmWithCorruption{ corruptCalibrationRegion, corruptConfigRegion, faultHistoryRegionStub, learnedModelRegion };

    corruptCalibrationRegion.corruptOnRead = true;

    bool done = false;
    services::NvmStatus result{};

    nvmWithCorruption.SaveCalibration(MakeTestCalibration(), [&](services::NvmStatus s)
        {
            result = s;
            done = true;
        });

    RunUntilDone(done);
    EXPECT_EQ(result, services::NvmStatus::HardwareFault);
}

TEST_F(NonVolatileMemoryTest, save_config_write_verify_failure_returns_hardware_fault)
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
    };

    NvmRegionStub normalCalibrationRegion{ services::CalibrationRecord::size };
    CorruptingRegion corruptConfigRegion{ services::ConfigRecord::size };
    NvmRegionStub normalFaultHistoryRegion{ services::faultHistoryImageSize };
    services::NonVolatileMemoryImpl nvmWithCorruption{ normalCalibrationRegion, corruptConfigRegion, normalFaultHistoryRegion, learnedModelRegion };

    bool done = false;
    services::NvmStatus result{};

    nvmWithCorruption.SaveConfig(MakeTestConfig(), [&](services::NvmStatus s)
        {
            result = s;
            done = true;
        });

    RunUntilDone(done);
    EXPECT_EQ(result, services::NvmStatus::HardwareFault);
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

TEST_F(NonVolatileMemoryTest, a_saved_fault_history_loads_back)
{
    services::FaultHistoryData history;
    history.powerUps = 4;
    history.records[2] = services::FaultRecord{ 2, 1, 4, 1500, 4000, 120 };
    std::optional<services::NvmStatus> saved;
    nvm.SaveFaultHistory(history, [&saved](services::NvmStatus status)
        {
            saved = status;
        });
    ExecuteAllActions();
    ASSERT_EQ(services::NvmStatus::Ok, saved);

    services::FaultHistoryData loaded;
    std::optional<services::NvmStatus> status;
    nvm.LoadFaultHistory(loaded, [&status](services::NvmStatus result)
        {
            status = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(services::NvmStatus::Ok, status);
    EXPECT_EQ(4, loaded.powerUps);
    EXPECT_EQ(history.records, loaded.records);
}

TEST_F(NonVolatileMemoryTest, a_blank_fault_history_region_loads_as_invalid_and_empty)
{
    services::FaultHistoryData loaded;
    loaded.powerUps = 9;
    loaded.records[0] = services::FaultRecord{ 2, 1, 1, 1, 1, 1 };
    std::optional<services::NvmStatus> status;

    nvm.LoadFaultHistory(loaded, [&status](services::NvmStatus result)
        {
            status = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(services::NvmStatus::InvalidData, status);
    EXPECT_EQ(0, loaded.powerUps);
    EXPECT_FALSE(loaded.records[0].has_value());
}

TEST_F(NonVolatileMemoryTest, a_fault_history_save_is_refused_while_a_config_save_runs)
{
    nvm.SaveConfig(services::ConfigData{}, [](services::NvmStatus) {});
    std::optional<services::NvmStatus> status;

    nvm.SaveFaultHistory(services::FaultHistoryData{}, [&status](services::NvmStatus result)
        {
            status = result;
        });

    EXPECT_EQ(services::NvmStatus::Busy, status);
    ExecuteAllActions();
}

TEST_F(NonVolatileMemoryTest, a_fault_history_save_whose_read_back_differs_reports_a_hardware_fault)
{
    struct ReadBackCorruptingRegion
        : NvmRegionStub
    {
        using NvmRegionStub::NvmRegionStub;

        void Read(infra::ByteRange data, infra::Function<void()> onDone) override
        {
            std::copy(storage.begin(), storage.begin() + data.size(), data.begin());
            data[0] ^= 0xFF;
            infra::EventDispatcher::Instance().Schedule(onDone);
        }
    };

    ReadBackCorruptingRegion corruptFaultHistoryRegion{ services::faultHistoryImageSize };
    services::NonVolatileMemoryImpl nvmWithCorruption{ calibrationRegion, configRegion, corruptFaultHistoryRegion, learnedModelRegion };
    std::optional<services::NvmStatus> status;

    nvmWithCorruption.SaveFaultHistory(services::FaultHistoryData{}, [&status](services::NvmStatus result)
        {
            status = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(services::NvmStatus::HardwareFault, status);
}

TEST_F(NonVolatileMemoryTest, a_saved_learned_model_loads_back)
{
    const services::LearnedModelData model{ 0xCAFE0001u, 1.1f, std::nullopt, 2.0e-6f };
    std::optional<services::NvmStatus> saved;
    nvm.SaveLearnedModel(model, [&saved](services::NvmStatus status)
        {
            saved = status;
        });
    ExecuteAllActions();
    ASSERT_EQ(services::NvmStatus::Ok, saved);

    services::LearnedModelData loaded;
    std::optional<services::NvmStatus> status;
    nvm.LoadLearnedModel(loaded, [&status](services::NvmStatus result)
        {
            status = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(services::NvmStatus::Ok, status);
    EXPECT_EQ(model, loaded);
}

TEST_F(NonVolatileMemoryTest, a_blank_learned_model_region_loads_as_invalid_and_empty)
{
    services::LearnedModelData loaded{ 7u, 1.0f, 1.0f, 1.0f };
    std::optional<services::NvmStatus> status;

    nvm.LoadLearnedModel(loaded, [&status](services::NvmStatus result)
        {
            status = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(services::NvmStatus::InvalidData, status);
    EXPECT_EQ(services::LearnedModelData{}, loaded);
}

TEST_F(NonVolatileMemoryTest, a_learned_model_save_is_refused_while_a_config_save_runs)
{
    nvm.SaveConfig(services::ConfigData{}, [](services::NvmStatus) {});
    std::optional<services::NvmStatus> status;

    nvm.SaveLearnedModel(services::LearnedModelData{}, [&status](services::NvmStatus result)
        {
            status = result;
        });

    EXPECT_EQ(services::NvmStatus::Busy, status);
    ExecuteAllActions();
}
