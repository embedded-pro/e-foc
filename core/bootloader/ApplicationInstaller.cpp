#include "core/bootloader/ApplicationInstaller.hpp"
#include "core/bootloader/ApplicationCheck.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "services/crypto/Sha256Software.hpp"
#include "upgrade/pack/UpgradePackHeader.hpp"
#include <algorithm>
#include <array>

namespace bootloader
{
    namespace
    {
        constexpr uint32_t initialCounterSize = 16;
    }

    ApplicationInstaller::ApplicationInstaller(application::Decryptor& decryptor, application::Decryptor& checkDecryptor, hal::SynchronousFlash& slot,
        const TargetLayout& layout, const TargetIdentity& identity, infra::ByteRange buffer)
        : application::ImageUpgrader("app", decryptor)
        , checkDecryptor(checkDecryptor)
        , slot(slot)
        , layout(layout)
        , identity(identity)
        , buffer(buffer)
    {
        really_assert(buffer.size() >= imageStartCheckSize);
        really_assert(slot.TotalSize() == layout.slotEnd - layout.slotBegin);
    }

    uint32_t ApplicationInstaller::Upgrade(hal::SynchronousFlash& packFlash, uint32_t imageAddress, uint32_t imageSize, uint32_t destinationAddress)
    {
        if (destinationAddress != layout.slotBegin)
            return errorWrongDestination;

        if (!SizeFits(imageSize))
            return errorImageSize;

        if (!StartFitsTarget(packFlash, imageAddress, imageSize))
            return errorWrongTarget;

        EraseFor(imageSize);
        Copy(packFlash, imageAddress, imageSize);
        WriteDescriptor(imageSize);
        return 0;
    }

    bool ApplicationInstaller::SizeFits(uint32_t imageSize) const
    {
        return imageSize >= applicationHeaderOffset + sizeof(ApplicationHeader) && imageSize <= slot.TotalSize() - imageDescriptorSize;
    }

    bool ApplicationInstaller::StartFitsTarget(hal::SynchronousFlash& packFlash, uint32_t imageAddress, uint32_t imageSize)
    {
        packFlash.ReadBuffer(infra::Head(checkDecryptor.StateBuffer(), initialCounterSize), imageAddress - sizeof(application::ImageHeaderEpilogue) - initialCounterSize);
        checkDecryptor.Reset();

        application::ImageHeaderEpilogue epilogue{};
        checkDecryptor.DecryptPart(infra::MakeByteRange(epilogue));

        auto start = infra::Head(buffer, std::min(imageSize, imageStartCheckSize));
        packFlash.ReadBuffer(start, imageAddress);
        checkDecryptor.DecryptPart(start);

        return ImageStartFitsTarget(start, layout, identity);
    }

    void ApplicationInstaller::EraseFor(uint32_t imageSize)
    {
        const auto lastImageSector = slot.SectorOfAddress(imageSize - 1);
        const auto descriptorSector = slot.SectorOfAddress(slot.TotalSize() - imageDescriptorSize);

        slot.EraseSectors(0, lastImageSector + 1);
        if (descriptorSector > lastImageSector)
            slot.EraseSector(descriptorSector);
    }

    void ApplicationInstaller::Copy(hal::SynchronousFlash& packFlash, uint32_t imageAddress, uint32_t imageSize)
    {
        for (uint32_t offset = 0; offset < imageSize;)
        {
            auto chunk = infra::Head(buffer, imageSize - offset);
            packFlash.ReadBuffer(chunk, imageAddress + offset);
            ImageDecryptor().DecryptPart(chunk);
            slot.WriteBuffer(chunk, offset);
            offset += chunk.size();
        }
    }

    void ApplicationInstaller::WriteDescriptor(uint32_t imageSize)
    {
        services::Sha256SoftwareIncremental sha256;

        for (uint32_t offset = 0; offset < imageSize;)
        {
            auto chunk = infra::Head(buffer, imageSize - offset);
            slot.ReadBuffer(chunk, offset);
            sha256.Update(chunk);
            offset += chunk.size();
        }

        ImageDescriptor descriptor{ ImageDescriptor::validMagic, imageSize, sha256.Finish(), {} };
        descriptor.reserved.fill(0xff);
        slot.WriteBuffer(infra::MakeByteRange(descriptor), slot.TotalSize() - imageDescriptorSize);
    }
}
