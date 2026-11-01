#pragma once

#include <array>
#include <cstdint>

namespace bootloader
{
    inline constexpr uint32_t applicationHeaderOffset = 0x400;
    inline constexpr uint32_t imageDescriptorSize = 64;
    inline constexpr uint32_t imageStartCheckSize = 2048;

    enum class DeviceFamily : uint16_t
    {
        tivaTm4c = 1,
        stm32 = 2,
    };

    struct TargetIdentity
    {
        uint16_t boardKind;
        DeviceFamily family;
        uint32_t device;

        bool operator==(const TargetIdentity& other) const = default;
    };

    struct TargetLayout
    {
        uint32_t ramBegin;
        uint32_t ramEnd;
        uint32_t slotBegin;
        uint32_t slotEnd;
    };

    struct ApplicationHeader
    {
        static constexpr uint32_t validMagic = 0x48414645;
        static constexpr uint16_t currentVersion = 1;

        uint32_t magic;
        uint16_t headerVersion;
        uint16_t boardKind;
        uint16_t deviceFamily;
        uint16_t reserved0;
        uint32_t device;
        uint16_t versionMajor;
        uint16_t versionMinor;
        uint16_t versionPatch;
        uint16_t reserved1;
        std::array<uint8_t, 40> reserved;
    };

    static_assert(sizeof(ApplicationHeader) == 64);

    constexpr ApplicationHeader MakeApplicationHeader(const TargetIdentity& identity, uint16_t major, uint16_t minor, uint16_t patch)
    {
        ApplicationHeader header{ ApplicationHeader::validMagic, ApplicationHeader::currentVersion, identity.boardKind,
            static_cast<uint16_t>(identity.family), 0xffff, identity.device, major, minor, patch, 0xffff, {} };
        header.reserved.fill(0xff);
        return header;
    }

    struct ImageDescriptor
    {
        static constexpr uint32_t validMagic = 0x44494645;

        uint32_t magic;
        uint32_t imageSize;
        std::array<uint8_t, 32> sha256;
        std::array<uint8_t, 24> reserved;
    };

    static_assert(sizeof(ImageDescriptor) == imageDescriptorSize);
}
