#pragma once

#include "core/bootloader/ApplicationCheck.hpp"
#include "core/bootloader/ImageFormat.hpp"
#include "infra/util/ByteRange.hpp"
#include <cstdint>
#include <cstring>
#include <vector>

namespace bootloader
{
    inline constexpr TargetLayout tm4c1294Layout{ 0x20000000, 0x20040000, 0x00008000, 0x00084000 };
    inline constexpr TargetIdentity tm4c1294Identity{ 1, DeviceFamily::tivaTm4c, 0x0a };
    inline constexpr TargetIdentity stm32h563Identity{ 3, DeviceFamily::stm32, 0x484 };

    inline void Put32(std::vector<uint8_t>& bytes, std::size_t offset, uint32_t value)
    {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    inline std::vector<uint8_t> Image(const TargetIdentity& identity, uint32_t stackPointer, uint32_t resetHandler, std::size_t size = 4096, uint8_t fill = 0x5a)
    {
        std::vector<uint8_t> image(size, fill);
        Put32(image, 0, stackPointer);
        Put32(image, 4, resetHandler);
        const auto header = MakeApplicationHeader(identity, 1, 7, 0);
        std::memcpy(image.data() + applicationHeaderOffset, &header, sizeof(header));
        return image;
    }

    inline std::vector<uint8_t> GoodImage(std::size_t size = 4096, uint8_t fill = 0x5a)
    {
        return Image(tm4c1294Identity, 0x20040000, 0x00008441, size, fill);
    }

    inline std::vector<uint8_t> Slot(const std::vector<uint8_t>& image, std::size_t slotSize)
    {
        std::vector<uint8_t> slot(slotSize, 0xff);
        std::copy(image.begin(), image.end(), slot.begin());
        const auto descriptor = MakeImageDescriptor(infra::MakeRange(image));
        std::memcpy(slot.data() + slot.size() - sizeof(descriptor), &descriptor, sizeof(descriptor));
        return slot;
    }
}
