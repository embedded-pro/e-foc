#include "KeySlot.hpp"

namespace bootloader
{
    [[gnu::section(".bootloader_keys"), gnu::used]] const KeySlot keySlot{ ErasedKeySlot() };
}
