#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "infra/util/Crc.hpp"
#include <cassert>
#include <cstring>

namespace services
{
    namespace
    {
        // Layout: [magic:4][version:1][crc32:4][data:N]
        // pragma pack prevents inter-field padding so the CRC covers exactly the stored bytes.
#pragma pack(push, 1)

        struct CalibrationStorageRecord
        {
            uint32_t magic;
            uint8_t version;
            uint32_t crc32;
            CalibrationData data;
        };

        struct ConfigStorageRecord
        {
            uint32_t magic;
            uint8_t version;
            uint32_t crc32;
            ConfigData data;
        };

#pragma pack(pop)

        static_assert(sizeof(CalibrationStorageRecord) == NonVolatileMemoryImpl::calibrationRecordSize,
            "calibrationRecordSize must match the packed record — check CalibrationData for implicit padding");
        static_assert(sizeof(ConfigStorageRecord) == NonVolatileMemoryImpl::configRecordSize,
            "configRecordSize must match the packed record — check ConfigData for implicit padding");
    }

    NonVolatileMemoryImpl::NonVolatileMemoryImpl(NvmRegion& calibrationRegion, NvmRegion& configRegion)
        : calibrationRegion(calibrationRegion)
        , configRegion(configRegion)
    {
    }

    uint32_t NonVolatileMemoryImpl::ComputeCrc(infra::ConstByteRange bytes)
    {
        infra::Crc32 crc;
        crc.Update(bytes);
        return crc.Result();
    }

    void NonVolatileMemoryImpl::CompleteCalibration(NvmStatus status)
    {
        busy = false;
        onCalibrationDone(status);
    }

    void NonVolatileMemoryImpl::CompleteConfig(NvmStatus status)
    {
        busy = false;
        onConfigDone(status);
    }

    void NonVolatileMemoryImpl::CompleteFormat(NvmStatus status)
    {
        busy = false;
        onFormatDone(status);
    }

    void NonVolatileMemoryImpl::CompleteIsCalibrationValid(bool valid)
    {
        busy = false;
        onIsCalibrationValidDone(valid);
    }

    void NonVolatileMemoryImpl::SaveCalibration(const CalibrationData& data,
        infra::Function<void(NvmStatus)> onDone)
    {
        if (busy)
        {
            onDone(NvmStatus::Busy);
            return;
        }

        busy = true;

        CalibrationStorageRecord record{};
        record.magic = CalibrationMagic;
        record.version = CalibrationLayoutVersion;
        record.data = data;
        record.crc32 = ComputeCrc(infra::MakeConstByteRange(record.data));
        std::memcpy(calibrationBuffer.data(), &record, sizeof(record));

        onCalibrationDone = onDone;
        calibrationRegion.Erase([this]
            {
                OnCalibrationErased();
            });
    }

    void NonVolatileMemoryImpl::OnCalibrationErased()
    {
        calibrationRegion.Write(
            infra::ConstByteRange(calibrationBuffer),
            [this]
            {
                OnCalibrationWritten();
            });
    }

    void NonVolatileMemoryImpl::OnCalibrationWritten()
    {
        calibrationRegion.Read(
            infra::ByteRange(calibrationReadBackBuffer),
            [this]
            {
                OnCalibrationReadBack();
            });
    }

    void NonVolatileMemoryImpl::OnCalibrationReadBack()
    {
        if (calibrationBuffer == calibrationReadBackBuffer)
            CompleteCalibration(NvmStatus::Ok);
        else
            CompleteCalibration(NvmStatus::HardwareFault);
    }

    void NonVolatileMemoryImpl::LoadCalibration(CalibrationData& data,
        infra::Function<void(NvmStatus)> onDone)
    {
        if (busy)
        {
            onDone(NvmStatus::Busy);
            return;
        }

        busy = true;

        pendingCalibrationOutput = &data;
        onCalibrationDone = onDone;

        calibrationRegion.Read(
            infra::ByteRange(calibrationReadBackBuffer),
            [this]
            {
                OnCalibrationReadForLoad();
            });
    }

    void NonVolatileMemoryImpl::OnCalibrationReadForLoad()
    {
        CalibrationStorageRecord record{};
        std::memcpy(&record, calibrationReadBackBuffer.data(), sizeof(record));

        if (record.magic != CalibrationMagic)
        {
            *pendingCalibrationOutput = CalibrationData{};
            CompleteCalibration(NvmStatus::InvalidData);
            return;
        }

        if (record.version != CalibrationLayoutVersion)
        {
            *pendingCalibrationOutput = CalibrationData{};
            CompleteCalibration(NvmStatus::VersionMismatch);
            return;
        }

        const uint32_t computed = ComputeCrc(infra::MakeConstByteRange(record.data));
        if (computed != record.crc32)
        {
            *pendingCalibrationOutput = CalibrationData{};
            CompleteCalibration(NvmStatus::InvalidData);
            return;
        }

        *pendingCalibrationOutput = record.data;
        CompleteCalibration(NvmStatus::Ok);
    }

    void NonVolatileMemoryImpl::InvalidateCalibration(infra::Function<void(NvmStatus)> onDone)
    {
        if (busy)
        {
            onDone(NvmStatus::Busy);
            return;
        }

        busy = true;

        onCalibrationDone = onDone;
        calibrationRegion.Erase([this]
            {
                calibrationRegion.Read(
                    infra::ByteRange(calibrationReadBackBuffer),
                    [this]
                    {
                        OnCalibrationInvalidationVerified();
                    });
            });
    }

    void NonVolatileMemoryImpl::IsCalibrationValid(infra::Function<void(bool)> onDone)
    {
        if (busy)
        {
            onDone(false);
            return;
        }

        busy = true;

        onIsCalibrationValidDone = onDone;

        calibrationRegion.Read(
            infra::ByteRange(calibrationReadBackBuffer),
            [this]
            {
                OnCalibrationReadForIsValid();
            });
    }

    void NonVolatileMemoryImpl::OnCalibrationReadForIsValid()
    {
        CalibrationStorageRecord record{};
        std::memcpy(&record, calibrationReadBackBuffer.data(), sizeof(record));

        if (record.magic != CalibrationMagic || record.version != CalibrationLayoutVersion)
        {
            CompleteIsCalibrationValid(false);
            return;
        }

        const uint32_t computed = ComputeCrc(infra::MakeConstByteRange(record.data));
        CompleteIsCalibrationValid(computed == record.crc32);
    }

    void NonVolatileMemoryImpl::SaveConfig(const ConfigData& data,
        infra::Function<void(NvmStatus)> onDone)
    {
        if (busy)
        {
            onDone(NvmStatus::Busy);
            return;
        }

        busy = true;

        ConfigStorageRecord record{};
        record.magic = ConfigMagic;
        record.version = ConfigLayoutVersion;
        record.data = data;
        record.crc32 = ComputeCrc(infra::MakeConstByteRange(record.data));
        std::memcpy(configBuffer.data(), &record, sizeof(record));

        onConfigDone = onDone;
        configRegion.Erase([this]
            {
                OnConfigErased();
            });
    }

    void NonVolatileMemoryImpl::OnConfigErased()
    {
        configRegion.Write(
            infra::ConstByteRange(configBuffer),
            [this]
            {
                OnConfigWritten();
            });
    }

    void NonVolatileMemoryImpl::OnConfigWritten()
    {
        configRegion.Read(
            infra::ByteRange(configReadBackBuffer),
            [this]
            {
                OnConfigReadBack();
            });
    }

    void NonVolatileMemoryImpl::OnConfigReadBack()
    {
        if (configBuffer == configReadBackBuffer)
            CompleteConfig(NvmStatus::Ok);
        else
            CompleteConfig(NvmStatus::HardwareFault);
    }

    void NonVolatileMemoryImpl::LoadConfig(ConfigData& data,
        infra::Function<void(NvmStatus)> onDone)
    {
        if (busy)
        {
            onDone(NvmStatus::Busy);
            return;
        }

        busy = true;

        pendingConfigOutput = &data;
        onConfigDone = onDone;

        configRegion.Read(
            infra::ByteRange(configReadBackBuffer),
            [this]
            {
                OnConfigReadForLoad();
            });
    }

    void NonVolatileMemoryImpl::OnConfigReadForLoad()
    {
        ConfigStorageRecord record{};
        std::memcpy(&record, configReadBackBuffer.data(), sizeof(record));

        if (record.magic != ConfigMagic || record.version != ConfigLayoutVersion)
        {
            *pendingConfigOutput = MakeDefaultConfigData();
            CompleteConfig(NvmStatus::Ok);
            return;
        }

        const uint32_t computed = ComputeCrc(infra::MakeConstByteRange(record.data));
        if (computed != record.crc32)
        {
            *pendingConfigOutput = MakeDefaultConfigData();
            CompleteConfig(NvmStatus::Ok);
            return;
        }

        *pendingConfigOutput = record.data;
        CompleteConfig(NvmStatus::Ok);
    }

    void NonVolatileMemoryImpl::ResetConfigToDefaults(infra::Function<void(NvmStatus)> onDone)
    {
        SaveConfig(MakeDefaultConfigData(), onDone);
    }

    void NonVolatileMemoryImpl::Format(infra::Function<void(NvmStatus)> onDone)
    {
        if (busy)
        {
            onDone(NvmStatus::Busy);
            return;
        }

        busy = true;

        onFormatDone = onDone;
        calibrationRegion.Erase([this]
            {
                OnCalibrationSectorFormattedDuringFormat();
            });
    }

    void NonVolatileMemoryImpl::OnCalibrationInvalidationVerified()
    {
        uint32_t magic{};
        std::memcpy(&magic, calibrationReadBackBuffer.data(), sizeof(magic));
        CompleteCalibration(magic == CalibrationMagic ? NvmStatus::WriteFailed : NvmStatus::Ok);
    }

    void NonVolatileMemoryImpl::OnCalibrationSectorFormattedDuringFormat()
    {
        calibrationRegion.Read(
            infra::ByteRange(calibrationReadBackBuffer),
            [this]
            {
                OnCalibrationFormatVerified();
            });
    }

    void NonVolatileMemoryImpl::OnCalibrationFormatVerified()
    {
        uint32_t magic{};
        std::memcpy(&magic, calibrationReadBackBuffer.data(), sizeof(magic));
        if (magic == CalibrationMagic)
        {
            CompleteFormat(NvmStatus::WriteFailed);
            return;
        }

        configRegion.Erase([this]
            {
                configRegion.Read(
                    infra::ByteRange(configReadBackBuffer),
                    [this]
                    {
                        OnConfigSectorFormattedDuringFormat();
                    });
            });
    }

    void NonVolatileMemoryImpl::OnConfigSectorFormattedDuringFormat()
    {
        uint32_t magic{};
        std::memcpy(&magic, configReadBackBuffer.data(), sizeof(magic));
        CompleteFormat(magic == ConfigMagic ? NvmStatus::WriteFailed : NvmStatus::Ok);
    }
}
