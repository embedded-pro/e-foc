#include "KeySlot.hpp"

namespace bootloader
{
    // Written after the link (bootloader.md Part G); defined apart from its readers so the erased value is never folded
    [[gnu::section(".bootloader_keys"), gnu::used]] const KeySlot keySlot{ ErasedKeySlot() };
}
