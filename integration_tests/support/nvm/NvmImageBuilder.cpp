#include "integration_tests/support/nvm/NvmImageBuilder.hpp"
#include "core/services/non_volatile_memory/NvmRecord.hpp"
#include "infra/util/ByteRange.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"

namespace
{
    constexpr float kMilliPerUnit = 1000.0f;

    template<class Record, class Data>
    void WriteRecord(std::vector<uint8_t>& image, std::size_t offset, const Data& data, sil::RecordDamage damage)
    {
        const auto record = infra::ByteRange{ image.data() + offset, image.data() + offset + Record::size };
        Record::Serialize(data, record);

        switch (damage)
        {
            case sil::RecordDamage::badMagic:
                record[0] = static_cast<uint8_t>(~record[0]);
                break;
            case sil::RecordDamage::staleVersion:
                record[Record::versionOffset] = static_cast<uint8_t>(record[Record::versionOffset] - 1u);
                break;
            case sil::RecordDamage::corruptCrc:
                record[Record::crcOffset] = static_cast<uint8_t>(~record[Record::crcOffset]);
                break;
            case sil::RecordDamage::none:
                break;
        }
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
        data.fluxLinkage = motor.psi_f.Value();
        data.inertia = motor.J.Value();
        data.frictionViscous = motor.B.Value();
        data.frictionCoulomb = 0.0f;
        data.speedLoopBandwidth = 100.0f;
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
            WriteRecord<services::CalibrationRecord>(image, calibrationRegionOffset, spec.calibration, spec.calibrationDamage);

        if (spec.includeConfig)
            WriteRecord<services::ConfigRecord>(image, configRegionOffset, spec.config, spec.configDamage);

        return image;
    }
}
