#include "source/services/NonVolatileMemory/NonVolatileMemoryImpl.hpp"
#include "infra/util/Crc.hpp"
#include <cstring>

namespace services
{
    NonVolatileMemoryImpl::NonVolatileMemoryImpl(hal::Flash& flash)
        : flash(flash)
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
        auto& record = *reinterpret_cast<CalibrationStorageRecord*>(calibrationBuffer.data());
        record.magic = CalibrationMagic;
        record.version = CalibrationLayoutVersion;
        record.data = data;
        record.crc32 = ComputeCrc(infra::MakeConstByteRange(record.data));

        onCalibrationDone = std::move(onDone);
        flash.EraseSectors(calibrationSectorIndex, calibrationSectorIndex + 1,
            [this]
            {
                OnCalibrationErased();
            });
    }

    void NonVolatileMemoryImpl::OnCalibrationErased()
    {
        flash.WriteBuffer(
            infra::MakeConstByteRange(*reinterpret_cast<const CalibrationStorageRecord*>(calibrationBuffer.data())),
            flash.AddressOfSector(calibrationSectorIndex),
            [this]
            {
                OnCalibrationWritten();
            });
    }

    void NonVolatileMemoryImpl::OnCalibrationWritten()
    {
        flash.ReadBuffer(
            infra::MakeByteRange(*reinterpret_cast<CalibrationStorageRecord*>(calibrationReadBackBuffer.data())),
            flash.AddressOfSector(calibrationSectorIndex),
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
        pendingCalibrationOutput = &data;
        onCalibrationDone = std::move(onDone);

        flash.ReadBuffer(
            infra::MakeByteRange(*reinterpret_cast<CalibrationStorageRecord*>(calibrationReadBackBuffer.data())),
            flash.AddressOfSector(calibrationSectorIndex),
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

                const auto computed = ComputeCrc(infra::MakeConstByteRange(record.data));
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
        onCalibrationDone = std::move(onDone);
        flash.EraseSectors(calibrationSectorIndex, calibrationSectorIndex + 1,
            [this]
            {
                onCalibrationDone(NvmStatus::Ok);
            });
    }

    void NonVolatileMemoryImpl::IsCalibrationValid(infra::Function<void(bool)> onDone)
    {
        onIsCalibrationValidDone = std::move(onDone);

        flash.ReadBuffer(
            infra::MakeByteRange(*reinterpret_cast<CalibrationStorageRecord*>(calibrationReadBackBuffer.data())),
            flash.AddressOfSector(calibrationSectorIndex),
            [this]()
            {
                const auto& record = *reinterpret_cast<const CalibrationStorageRecord*>(
                    calibrationReadBackBuffer.data());

                if (record.magic != CalibrationMagic || record.version != CalibrationLayoutVersion)
                {
                    onIsCalibrationValidDone(false);
                    return;
                }

                const auto computed = ComputeCrc(infra::MakeConstByteRange(record.data));
                onIsCalibrationValidDone(computed == record.crc32);
            });
    }

    void NonVolatileMemoryImpl::SaveConfig(const ConfigData& data,
        infra::Function<void(NvmStatus)> onDone)
    {
        auto& record = *reinterpret_cast<ConfigStorageRecord*>(configBuffer.data());
        record.magic = ConfigMagic;
        record.version = ConfigLayoutVersion;
        record.data = data;
        record.crc32 = ComputeCrc(infra::MakeConstByteRange(record.data));

        onConfigDone = std::move(onDone);
        flash.EraseSectors(configSectorIndex, configSectorIndex + 1,
            [this]
            {
                OnConfigErased();
            });
    }

    void NonVolatileMemoryImpl::OnConfigErased()
    {
        flash.WriteBuffer(
            infra::MakeConstByteRange(*reinterpret_cast<const ConfigStorageRecord*>(configBuffer.data())),
            flash.AddressOfSector(configSectorIndex),
            [this]
            {
                OnConfigWritten();
            });
    }

    void NonVolatileMemoryImpl::OnConfigWritten()
    {
        flash.ReadBuffer(
            infra::MakeByteRange(*reinterpret_cast<ConfigStorageRecord*>(configReadBackBuffer.data())),
            flash.AddressOfSector(configSectorIndex),
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
        pendingConfigOutput = &data;
        onConfigDone = std::move(onDone);

        flash.ReadBuffer(
            infra::MakeByteRange(*reinterpret_cast<ConfigStorageRecord*>(configReadBackBuffer.data())),
            flash.AddressOfSector(configSectorIndex),
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

                const auto computed = ComputeCrc(infra::MakeConstByteRange(record.data));
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
        onFormatDone = std::move(onDone);
        flash.EraseSectors(calibrationSectorIndex, calibrationSectorIndex + 1,
            [this]
            {
                OnCalibrationSectorFormattedDuringFormat();
            });
    }

    void NonVolatileMemoryImpl::OnCalibrationSectorFormattedDuringFormat()
    {
        flash.EraseSectors(configSectorIndex, configSectorIndex + 1,
            [this]
            {
                onFormatDone(NvmStatus::Ok);
            });
    }
}
