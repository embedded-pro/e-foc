#include "source/services/NonVolatileMemory/NonVolatileMemoryImpl.hpp"
#include "infra/util/Crc.hpp"
#include <cassert>
#include <cstring>

namespace services
{
    namespace
    {
        // Storage records are private to this translation unit.
        // Layout: [magic:4][version:1][crc32:4][data:N]
        // The data structs are padding-free by design (see static_asserts in their headers),
        // so no #pragma pack is needed — every byte stored is a meaningful field byte.
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

    void NonVolatileMemoryImpl::SaveCalibration(const CalibrationData& data,
        infra::Function<void(NvmStatus)> onDone)
    {
        if (onCalibrationDone)
            return;

        auto& record = *reinterpret_cast<CalibrationStorageRecord*>(calibrationBuffer.data());
        record.magic = CalibrationMagic;
        record.version = CalibrationLayoutVersion;
        record.data = data;
        record.crc32 = ComputeCrc(infra::MakeConstByteRange(record.data));

        onCalibrationDone = std::move(onDone);
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
            onCalibrationDone(NvmStatus::Ok);
        else
            onCalibrationDone(NvmStatus::WriteFailed);
    }

    void NonVolatileMemoryImpl::LoadCalibration(CalibrationData& data,
        infra::Function<void(NvmStatus)> onDone)
    {
        if (onCalibrationDone)
            return;

        pendingCalibrationOutput = &data;
        onCalibrationDone = std::move(onDone);

        calibrationRegion.Read(
            infra::ByteRange(calibrationReadBackBuffer),
            [this]
            {
                const auto& record = *reinterpret_cast<const CalibrationStorageRecord*>(
                    calibrationReadBackBuffer.data());

                if (record.magic != CalibrationMagic)
                {
                    onCalibrationDone(NvmStatus::InvalidData);
                    return;
                }

                if (record.version != CalibrationLayoutVersion)
                {
                    onCalibrationDone(NvmStatus::VersionMismatch);
                    return;
                }

                const uint32_t computed = ComputeCrc(infra::MakeConstByteRange(record.data));
                if (computed != record.crc32)
                {
                    onCalibrationDone(NvmStatus::InvalidData);
                    return;
                }

                *pendingCalibrationOutput = record.data;
                onCalibrationDone(NvmStatus::Ok);
            });
    }

    void NonVolatileMemoryImpl::InvalidateCalibration(infra::Function<void(NvmStatus)> onDone)
    {
        if (onCalibrationDone)
            return;

        onCalibrationDone = std::move(onDone);
        calibrationRegion.Erase([this]
            {
                onCalibrationDone(NvmStatus::Ok);
            });
    }

    void NonVolatileMemoryImpl::IsCalibrationValid(infra::Function<void(bool)> onDone)
    {
        if (onIsCalibrationValidDone)
            return;

        onIsCalibrationValidDone = std::move(onDone);

        calibrationRegion.Read(
            infra::ByteRange(calibrationReadBackBuffer),
            [this]
            {
                const auto& record = *reinterpret_cast<const CalibrationStorageRecord*>(
                    calibrationReadBackBuffer.data());

                if (record.magic != CalibrationMagic || record.version != CalibrationLayoutVersion)
                {
                    onIsCalibrationValidDone(false);
                    return;
                }

                const uint32_t computed = ComputeCrc(infra::MakeConstByteRange(record.data));
                onIsCalibrationValidDone(computed == record.crc32);
            });
    }

    void NonVolatileMemoryImpl::SaveConfig(const ConfigData& data,
        infra::Function<void(NvmStatus)> onDone)
    {
        if (onConfigDone)
            return;

        auto& record = *reinterpret_cast<ConfigStorageRecord*>(configBuffer.data());
        record.magic = ConfigMagic;
        record.version = ConfigLayoutVersion;
        record.data = data;
        record.crc32 = ComputeCrc(infra::MakeConstByteRange(record.data));

        onConfigDone = std::move(onDone);
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
            onConfigDone(NvmStatus::Ok);
        else
            onConfigDone(NvmStatus::WriteFailed);
    }

    void NonVolatileMemoryImpl::LoadConfig(ConfigData& data,
        infra::Function<void(NvmStatus)> onDone)
    {
        if (onConfigDone)
            return;

        pendingConfigOutput = &data;
        onConfigDone = std::move(onDone);

        configRegion.Read(
            infra::ByteRange(configReadBackBuffer),
            [this]
            {
                const auto& record = *reinterpret_cast<const ConfigStorageRecord*>(
                    configReadBackBuffer.data());

                if (record.magic != ConfigMagic || record.version != ConfigLayoutVersion)
                {
                    *pendingConfigOutput = MakeDefaultConfigData();
                    onConfigDone(NvmStatus::Ok);
                    return;
                }

                const uint32_t computed = ComputeCrc(infra::MakeConstByteRange(record.data));
                if (computed != record.crc32)
                {
                    *pendingConfigOutput = MakeDefaultConfigData();
                    onConfigDone(NvmStatus::Ok);
                    return;
                }

                *pendingConfigOutput = record.data;
                onConfigDone(NvmStatus::Ok);
            });
    }

    void NonVolatileMemoryImpl::ResetConfigToDefaults(infra::Function<void(NvmStatus)> onDone)
    {
        SaveConfig(MakeDefaultConfigData(), std::move(onDone));
    }

    void NonVolatileMemoryImpl::Format(infra::Function<void(NvmStatus)> onDone)
    {
        if (onFormatDone)
            return;

        onFormatDone = std::move(onDone);
        calibrationRegion.Erase([this]
            {
                OnCalibrationSectorFormattedDuringFormat();
            });
    }

    void NonVolatileMemoryImpl::OnCalibrationSectorFormattedDuringFormat()
    {
        configRegion.Erase([this]
            {
                onFormatDone(NvmStatus::Ok);
            });
    }
}
