#include DEVICE_HEADER
#include "bootloader/ImageFormat.hpp"
#include "generated/Version.h"
#include "targets/bootloader/ImageTarget.hpp"
#include <array>
#include <cstdint>

extern "C" uint32_t _text;

namespace
{
    constexpr bootloader::ImageTarget target{ E_FOC_BOARD_KIND, static_cast<bootloader::DeviceFamily>(E_FOC_DEVICE_FAMILY), E_FOC_DEVICE_IDENTITY };

    constexpr std::array<uint8_t, bootloader::imageDescriptorSize> ErasedDescriptor()
    {
        std::array<uint8_t, bootloader::imageDescriptorSize> descriptor{};
        descriptor.fill(0xff);
        return descriptor;
    }

    [[gnu::section(".application_header"), gnu::used]] const bootloader::ApplicationHeader applicationHeader{
        bootloader::MakeApplicationHeader(target, e_foc::generated::VERSION_MAJOR, e_foc::generated::VERSION_MINOR, e_foc::generated::VERSION_PATCH)
    };

    [[gnu::section(".image_descriptor"), gnu::used]] const std::array<uint8_t, bootloader::imageDescriptorSize> imageDescriptor{ ErasedDescriptor() };

    [[gnu::constructor(101)]] void PointVectorTableAtImage()
    {
        SCB->VTOR = reinterpret_cast<uintptr_t>(&_text);
    }
}
