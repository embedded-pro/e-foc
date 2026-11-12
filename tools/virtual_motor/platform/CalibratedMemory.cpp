#include "tools/virtual_motor/platform/CalibratedMemory.hpp"
#include "core/foc/model/drive/memory/PlantCalibration.hpp"
#include <fstream>

namespace virtual_motor
{
    bool WriteCalibratedMemory(const std::string& memoryFile, const sil::SilPlantConfig& plant)
    {
        const auto image = sil::BuildNvmImage(sil::CalibratedFor(plant));
        std::ofstream file{ memoryFile, std::ios::binary | std::ios::trunc };
        file.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
        return file.good();
    }
}
