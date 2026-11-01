#include "core/bootloader/ApplicationCheck.hpp"
#include "core/bootloader/test/TestImages.hpp"
#include <gmock/gmock.h>
#include <vector>

namespace
{
    using namespace bootloader;

    constexpr uint32_t slotSize = 8 * 1024;

    std::vector<uint8_t> Slot(const std::vector<uint8_t>& image)
    {
        return bootloader::Slot(image, slotSize);
    }

    constexpr TargetLayout SmallSlotLayout()
    {
        return TargetLayout{ tm4c1294Layout.ramBegin, tm4c1294Layout.ramEnd, 0x00008000, 0x00008000 + slotSize };
    }
}

TEST(TestImageStart, an_image_for_this_chip_and_board_fits)
{
    EXPECT_TRUE(ImageStartFitsTarget(infra::MakeRange(GoodImage()), tm4c1294Layout, tm4c1294Identity));
}

TEST(TestImageStart, a_stack_pointer_outside_this_chips_ram_is_refused)
{
    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(Image(tm4c1294Identity, 0x20080000, 0x00008441)), tm4c1294Layout, tm4c1294Identity));
}

TEST(TestImageStart, a_reset_handler_outside_this_boards_slot_is_refused)
{
    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(Image(tm4c1294Identity, 0x20040000, 0x08008441)), tm4c1294Layout, tm4c1294Identity));
}

TEST(TestImageStart, a_reset_handler_without_the_thumb_bit_is_refused)
{
    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(Image(tm4c1294Identity, 0x20040000, 0x00008440)), tm4c1294Layout, tm4c1294Identity));
}

TEST(TestImageStart, an_image_for_another_microcontroller_family_is_refused)
{
    const TargetIdentity stm32{ 1, DeviceFamily::stm32, 0x484 };

    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(Image(stm32, 0x20040000, 0x00008441)), tm4c1294Layout, tm4c1294Identity));
}

TEST(TestImageStart, an_image_for_another_device_of_the_family_is_refused)
{
    const TargetIdentity tm4c123{ 1, DeviceFamily::tivaTm4c, 0x05 };

    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(Image(tm4c123, 0x20040000, 0x00008441)), tm4c1294Layout, tm4c1294Identity));
}

TEST(TestImageStart, an_image_for_another_board_is_refused)
{
    const TargetIdentity otherBoard{ 3, DeviceFamily::tivaTm4c, 0x0a };

    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(Image(otherBoard, 0x20040000, 0x00008441)), tm4c1294Layout, tm4c1294Identity));
}

TEST(TestImageStart, an_image_without_an_application_header_is_refused)
{
    auto image = GoodImage();
    image[applicationHeaderOffset] ^= 0xff;

    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(image), tm4c1294Layout, tm4c1294Identity));
}

TEST(TestImageStart, an_image_too_short_to_hold_the_header_is_refused)
{
    auto image = GoodImage();
    image.resize(applicationHeaderOffset + sizeof(ApplicationHeader) - 1);

    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(image), tm4c1294Layout, tm4c1294Identity));
}

TEST(TestStartable, an_intact_application_for_this_target_is_startable)
{
    const auto slot = Slot(GoodImage());

    EXPECT_TRUE(ApplicationIsStartable(infra::MakeRange(slot), SmallSlotLayout(), tm4c1294Identity));
}

TEST(TestStartable, a_changed_byte_in_the_image_is_not_startable)
{
    auto slot = Slot(GoodImage());
    slot[2000] ^= 0x01;

    EXPECT_FALSE(ApplicationIsStartable(infra::MakeRange(slot), SmallSlotLayout(), tm4c1294Identity));
}

TEST(TestStartable, an_erased_descriptor_is_not_startable)
{
    auto slot = Slot(GoodImage());
    std::fill(slot.end() - imageDescriptorSize, slot.end(), 0xff);

    EXPECT_FALSE(ApplicationIsStartable(infra::MakeRange(slot), SmallSlotLayout(), tm4c1294Identity));
}

TEST(TestStartable, a_descriptor_size_beyond_the_slot_is_not_startable)
{
    auto slot = Slot(GoodImage());
    Put32(slot, slot.size() - imageDescriptorSize + 4, slotSize);

    EXPECT_FALSE(ApplicationIsStartable(infra::MakeRange(slot), SmallSlotLayout(), tm4c1294Identity));
}

TEST(TestStartable, an_intact_image_for_another_microcontroller_is_not_startable)
{
    const auto slot = Slot(Image(stm32h563Identity, 0x20040000, 0x00008441));

    EXPECT_FALSE(ApplicationIsStartable(infra::MakeRange(slot), SmallSlotLayout(), tm4c1294Identity));
}

TEST(TestStartable, the_descriptor_covers_exactly_the_image_size)
{
    const auto image = GoodImage();
    const auto descriptor = MakeImageDescriptor(infra::MakeRange(image));

    EXPECT_EQ(ImageDescriptor::validMagic, descriptor.magic);
    EXPECT_EQ(image.size(), descriptor.imageSize);
}
