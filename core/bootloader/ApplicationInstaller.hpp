#pragma once

#include "core/bootloader/ImageFormat.hpp"
#include "hal/synchronous_interfaces/SynchronousFlash.hpp"
#include "infra/util/ByteRange.hpp"
#include "upgrade/boot_loader/Decryptor.hpp"
#include "upgrade/boot_loader/ImageUpgrader.hpp"

namespace bootloader
{
    inline constexpr uint32_t errorWrongTarget = 1000;
    inline constexpr uint32_t errorWrongDestination = 1001;
    inline constexpr uint32_t errorImageSize = 1002;

    class ApplicationInstaller
        : public application::ImageUpgrader
    {
    public:
        ApplicationInstaller(application::Decryptor& decryptor, application::Decryptor& checkDecryptor, hal::SynchronousFlash& slot,
            const TargetLayout& layout, const TargetIdentity& identity, infra::ByteRange buffer);

        uint32_t Upgrade(hal::SynchronousFlash& packFlash, uint32_t imageAddress, uint32_t imageSize, uint32_t destinationAddress) override;

    private:
        bool SizeFits(uint32_t imageSize) const;
        bool StartFitsTarget(hal::SynchronousFlash& packFlash, uint32_t imageAddress, uint32_t imageSize);
        void EraseFor(uint32_t imageSize);
        void Copy(hal::SynchronousFlash& packFlash, uint32_t imageAddress, uint32_t imageSize);
        void WriteDescriptor(uint32_t imageSize);

        application::Decryptor& checkDecryptor;
        hal::SynchronousFlash& slot;
        TargetLayout layout;
        TargetIdentity identity;
        infra::ByteRange buffer;
    };
}
