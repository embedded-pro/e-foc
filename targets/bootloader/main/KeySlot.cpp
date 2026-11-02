#include "KeySlot.hpp"

namespace bootloader
{
    namespace
    {
        constexpr KeySlot ErasedKeySlot()
        {
            KeySlot slot{};
            slot.signaturePublicKey.fill(0xff);
            slot.imageEncryptionKey.fill(0xff);
            return slot;
        }
    }

    // Written after the link (bootloader.md Part G); defined apart from its readers so the erased value is never folded
    [[gnu::section(".bootloader_keys"), gnu::used]] const KeySlot keySlot{ ErasedKeySlot() };
}
