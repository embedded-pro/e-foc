#include "BootloaderPlatform.hpp"
#include "core/bootloader/ApplicationInstaller.hpp"
#include "core/bootloader/BootSequence.hpp"
#include "core/bootloader/StatusJournal.hpp"
#include "targets/bootloader/keys/DevelopmentKeys.hpp"
#include "upgrade/boot_loader/DecryptorAesTiny.hpp"
#include "upgrade/boot_loader/VerifierEcDsa.hpp"
#include <array>
#include <cstdint>

extern "C"
{
    extern const uint8_t __application_slot_start[];
    extern const uint8_t __application_slot_end[];
    extern const uint8_t __staging_slot_start[];
    extern const uint8_t __staging_slot_end[];
    extern const uint8_t __status_journal_start[];
    extern const uint8_t __status_journal_end[];
    extern const uint8_t __ram_start[];
    extern const uint8_t __ram_end[];
}

namespace
{
    uint32_t Address(const uint8_t* symbol)
    {
        return reinterpret_cast<uintptr_t>(symbol);
    }

    infra::ConstByteRange Region(const uint8_t* begin, const uint8_t* end)
    {
        return infra::ConstByteRange{ begin, end };
    }
}

int main()
{
    const auto slot = Region(__application_slot_start, __application_slot_end);
    const bootloader::TargetLayout layout{ Address(__ram_start), Address(__ram_end), Address(__application_slot_start), Address(__application_slot_end) };

    bootloader::BootloaderPlatform platform{ Region(__staging_slot_start, __staging_slot_end), Region(__status_journal_start, __status_journal_end), slot };
    const auto identity = platform.Identity();

    application::DecryptorAesTiny decryptor{ infra::MakeRange(bootloader::keys::imageEncryptionKey) };
    application::DecryptorAesTiny checkDecryptor{ infra::MakeRange(bootloader::keys::imageEncryptionKey) };
    const application::VerifierEcDsa256 verifier{ infra::MakeRange(bootloader::keys::signaturePublicKey) };
    std::array<uint8_t, bootloader::imageStartCheckSize> buffer{};

    bootloader::StatusJournal journal{ platform.JournalFlash() };
    bootloader::ApplicationInstaller installer{ decryptor, checkDecryptor, platform.SlotFlash(), layout, identity, infra::MakeRange(buffer) };

    bootloader::BootSequence{ { platform.PackFlash(), journal, installer, decryptor, verifier, E_FOC_PRODUCT_NAME, slot, layout, identity, platform } }.Run();

    for (;;)
    {
    }
}
