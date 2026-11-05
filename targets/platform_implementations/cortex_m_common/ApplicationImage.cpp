#include "bootloader/ImageFormat.hpp"
#include "generated/Boards.hpp"
#include "generated/Version.h"
#include "targets/platform_implementations/cortex_m_common/ImageTarget.hpp"
#include <array>
#include <cstdint>

extern "C" uint32_t _text;

namespace
{
    constexpr uintptr_t vtorAddress = 0xE000ED08u;

    constexpr bootloader::ImageTarget target{ e_foc::board.kind, static_cast<bootloader::DeviceFamily>(e_foc::deviceFamily), e_foc::deviceIdentity };

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
        *reinterpret_cast<volatile uint32_t*>(vtorAddress) = reinterpret_cast<uintptr_t>(&_text);
    }
}
