#include "integration_tests/support/nvm/NvmImageBuilder.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Crc.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include <cstring>

namespace
{
    constexpr float kMilliPerUnit = 1000.0f;

    void WriteUint32(std::vector<uint8_t>& image, std::size_t offset, uint32_t value)
    {
        std::memcpy(image.data() + offset, &value, sizeof(value));
    }

    template<class Data>
    void WriteRecord(std::vector<uint8_t>& image, std::size_t offset, uint32_t magic,
        uint8_t version, const Data& data, sil::RecordDamage damage)
    {
        infra::Crc32 crc;
        crc.Update(infra::MakeConstByteRange(data));
        uint32_t checksum = crc.Result();

        switch (damage)
        {
            case sil::RecordDamage::badMagic:
                magic = ~magic;
                break;
            case sil::RecordDamage::staleVersion:
                version = static_cast<uint8_t>(version - 1u);
                break;
            case sil::RecordDamage::corruptCrc:
                checksum = ~checksum;
                break;
            case sil::RecordDamage::none:
                break;
        }

        WriteUint32(image, offset, magic);
        image[offset + sizeof(uint32_t)] = version;
        WriteUint32(image, offset + sizeof(uint32_t) + sizeof(uint8_t), checksum);
        std::memcpy(image.data() + offset + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t),
            &data, sizeof(data));
    }
}

namespace sil
{
    services::CalibrationData CompleteCalibration()
    {
        const auto& motor = foc::M_2310P_LN_04K::parameters;

        services::CalibrationData data{};
        data.rPhase = motor.R.Value();
        data.lD = motor.Ld.Value() * kMilliPerUnit;
        data.lQ = motor.Lq.Value() * kMilliPerUnit;
        data.fluxLinkage = motor.psi_f.Value();
        data.inertia = motor.J.Value();
        data.frictionViscous = motor.B.Value();
        data.frictionCoulomb = 0.0f;
        data.currentLoopBandwidth = 2000.0f;
        data.speedLoopBandwidth = 100.0f;
        data.encoderZeroOffset = 0;
        data.encoderDirection = 0;
        data.polePairs = motor.p;
        data.stage = services::CalibrationStage::complete;
        return data;
    }

    std::vector<uint8_t> BlankNvmImage()
    {
        return std::vector<uint8_t>(nvmImageSize, 0xFF);
    }

    std::vector<uint8_t> BuildNvmImage(const NvmImageSpec& spec)
    {
        auto image = BlankNvmImage();

        if (spec.includeCalibration)
            WriteRecord(image, calibrationRegionOffset, services::CalibrationMagic,
                services::CalibrationLayoutVersion, spec.calibration, spec.calibrationDamage);

        if (spec.includeConfig)
            WriteRecord(image, configRegionOffset, services::ConfigMagic,
                services::ConfigLayoutVersion, spec.config, spec.configDamage);

        return image;
    }
}
