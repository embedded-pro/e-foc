#include "core/bootloader/ApplicationCheck.hpp"
#include "services/crypto/Sha256Software.hpp"
#include <cstring>

namespace bootloader
{
    namespace
    {
        constexpr uint32_t thumbBit = 1;

        template<class T>
        T Read(infra::ConstByteRange bytes, std::size_t offset)
        {
            T value{};
            std::memcpy(&value, bytes.begin() + offset, sizeof(value));
            return value;
        }

        bool VectorsFitTarget(infra::ConstByteRange imageStart, const TargetLayout& layout)
        {
            const auto stackPointer = Read<uint32_t>(imageStart, 0);
            const auto resetHandler = Read<uint32_t>(imageStart, 4);
            const auto resetAddress = resetHandler & ~thumbBit;

            return stackPointer > layout.ramBegin && stackPointer <= layout.ramEnd && (resetHandler & thumbBit) != 0 &&
                   resetAddress >= layout.slotBegin && resetAddress < layout.slotEnd - imageDescriptorSize;
        }

        bool HeaderNamesTarget(infra::ConstByteRange imageStart, const TargetIdentity& identity)
        {
            const auto header = Read<ApplicationHeader>(imageStart, applicationHeaderOffset);

            return header.magic == ApplicationHeader::validMagic && header.headerVersion == ApplicationHeader::currentVersion &&
                   TargetIdentity{ header.boardKind, static_cast<DeviceFamily>(header.deviceFamily), header.device } == identity;
        }
    }

    bool ImageStartFitsTarget(infra::ConstByteRange imageStart, const TargetLayout& layout, const TargetIdentity& identity)
    {
        return imageStart.size() >= applicationHeaderOffset + sizeof(ApplicationHeader) && VectorsFitTarget(imageStart, layout) &&
               HeaderNamesTarget(imageStart, identity);
    }

    bool ApplicationIsStartable(infra::ConstByteRange slot, const TargetLayout& layout, const TargetIdentity& identity)
    {
        if (slot.size() < imageDescriptorSize)
            return false;

        const auto descriptor = Read<ImageDescriptor>(slot, slot.size() - imageDescriptorSize);
        if (descriptor.magic != ImageDescriptor::validMagic || descriptor.imageSize > slot.size() - imageDescriptorSize)
            return false;

        const auto image = infra::Head(slot, descriptor.imageSize);
        return services::Sha256Software{}.Calculate(image) == descriptor.sha256 && ImageStartFitsTarget(image, layout, identity);
    }

    ImageDescriptor MakeImageDescriptor(infra::ConstByteRange image)
    {
        ImageDescriptor descriptor{ ImageDescriptor::validMagic, static_cast<uint32_t>(image.size()), services::Sha256Software{}.Calculate(image), {} };
        descriptor.reserved.fill(0xff);
        return descriptor;
    }
}
