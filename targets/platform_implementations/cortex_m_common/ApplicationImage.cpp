#include DEVICE_HEADER
#include "core/bootloader/ImageFormat.hpp"
#include <array>
#include <cstdint>

extern "C" uint32_t _text;

namespace
{
    constexpr bootloader::TargetIdentity identity{ E_FOC_BOARD_KIND, static_cast<bootloader::DeviceFamily>(E_FOC_DEVICE_FAMILY), E_FOC_DEVICE_IDENTITY };

    constexpr std::array<uint8_t, bootloader::imageDescriptorSize> ErasedDescriptor()
    {
        std::array<uint8_t, bootloader::imageDescriptorSize> descriptor{};
        descriptor.fill(0xff);
        return descriptor;
    }

    [[gnu::section(".application_header"), gnu::used]] const bootloader::ApplicationHeader applicationHeader{
        bootloader::MakeApplicationHeader(identity, E_FOC_VERSION_MAJOR, E_FOC_VERSION_MINOR, E_FOC_VERSION_PATCH)
    };

    [[gnu::section(".image_descriptor"), gnu::used]] const std::array<uint8_t, bootloader::imageDescriptorSize> imageDescriptor{ ErasedDescriptor() };

    // A debugger may start the application without the bootloader; its interrupts must still find this vector table
    [[gnu::constructor(101)]] void PointVectorTableAtImage()
    {
        SCB->VTOR = reinterpret_cast<uintptr_t>(&_text);
    }
}
