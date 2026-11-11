#include "core/foc/model/drive/memory/PlantCalibration.hpp"
#include "core/services/non_volatile_memory/NvmRecord.hpp"
#include "motor_parameters/PlantPresets.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <cmath>

namespace
{
    std::optional<services::CalibrationData> CalibrationIn(const sil::NvmImage& image)
    {
        return services::CalibrationRecord::Deserialize(infra::ConstByteRange{ image.data() + sil::calibrationRegionOffset, image.data() + sil::calibrationRegionOffset + services::CalibrationRecord::size });
    }

    std::optional<services::ConfigData> ConfigIn(const sil::NvmImage& image)
    {
        return services::ConfigRecord::Deserialize(infra::ConstByteRange{ image.data() + sil::configRegionOffset, image.data() + sil::configRegionOffset + services::ConfigRecord::size });
    }

    sil::SilPlantConfig Teknic()
    {
        sil::SilPlantConfig plant{};
        EXPECT_TRUE(sil::TryNamedPlant("teknic", plant));
        return plant;
    }
}

TEST(TestNvmImageBuilder, a_blank_image_is_an_erased_memory)
{
    const auto image = sil::BlankNvmImage();

    EXPECT_EQ(image.size(), sil::nvmImageSize);
    EXPECT_TRUE(std::ranges::all_of(image, [](uint8_t byte)
        {
            return byte == 0xff;
        }));
}

TEST(TestNvmImageBuilder, the_records_read_back_as_written)
{
    sil::NvmImageSpec spec;
    spec.calibration = sil::CompleteCalibration();
    spec.config.currentLoop.bandwidth = sil::calibratedCurrentLoopBandwidth;

    const auto image = sil::BuildNvmImage(spec);

    ASSERT_TRUE(CalibrationIn(image).has_value());
    EXPECT_EQ(CalibrationIn(image)->stage, services::CalibrationStage::complete);
    ASSERT_TRUE(ConfigIn(image).has_value());
    EXPECT_FLOAT_EQ(ConfigIn(image)->currentLoop.bandwidth, sil::calibratedCurrentLoopBandwidth);
}

TEST(TestNvmImageBuilder, a_left_out_or_damaged_record_does_not_read_back)
{
    for (const auto damage : { sil::RecordDamage::badMagic, sil::RecordDamage::staleVersion, sil::RecordDamage::corruptCrc })
    {
        sil::NvmImageSpec spec;
        spec.calibrationDamage = damage;
        spec.includeConfig = false;

        const auto image = sil::BuildNvmImage(spec);

        EXPECT_FALSE(CalibrationIn(image).has_value());
        EXPECT_FALSE(ConfigIn(image).has_value());
    }
}

TEST(TestPlantCalibration, a_calibrated_motor_holds_its_plants_parameters_and_the_calibrated_bandwidth)
{
    auto plant = Teknic();
    plant.coulombFrictionNm = 0.01f;
    plant.noiseBiasAmpereB = -0.02f;

    const auto spec = sil::CalibratedFor(plant);

    EXPECT_TRUE(spec.includeCalibration);
    EXPECT_TRUE(spec.includeConfig);
    EXPECT_EQ(spec.calibration.stage, services::CalibrationStage::complete);
    EXPECT_EQ(spec.calibration.polePairs, plant.polePairs);
    EXPECT_FLOAT_EQ(spec.calibration.rPhase, plant.statorResistanceOhm);
    EXPECT_FLOAT_EQ(spec.calibration.lD, plant.dAxisInductanceHenry * 1000.0f);
    EXPECT_FLOAT_EQ(spec.calibration.fluxLinkage, plant.fluxLinkageWeber);
    EXPECT_FLOAT_EQ(spec.calibration.inertia, plant.rotorInertiaKgM2);
    EXPECT_FLOAT_EQ(spec.calibration.frictionViscous, plant.viscousDampingNmSPerRad);
    EXPECT_FLOAT_EQ(spec.calibration.frictionCoulomb, 0.01f);
    EXPECT_FLOAT_EQ(spec.calibration.currentOffsetB, -0.02f);
    EXPECT_FLOAT_EQ(spec.config.currentLoop.bandwidth, sil::calibratedCurrentLoopBandwidth);
}

TEST(TestPlantCalibration, the_encoder_index_angle_is_stored_only_for_an_encoder_with_an_index)
{
    auto plant = Teknic();
    plant.encoderIndexWidthCounts = 0;
    plant.encoderIndexRadians = 1.0f;
    EXPECT_TRUE(std::isnan(sil::CalibratedFor(plant).calibration.encoderIndexAngle));

    plant.encoderIndexWidthCounts = 4;
    plant.encoderIndexRadians = 7.0f;
    EXPECT_NEAR(sil::CalibratedFor(plant).calibration.encoderIndexAngle, 7.0f - 2.0f * 3.14159265f, 1e-5f);
}
