#include "integration_tests/support/interactor/hardware/GdbScript.hpp"
#include <filesystem>

namespace hil
{
    std::string FlashScript(const std::string& gdbTarget, const std::optional<std::string>& bootloaderElf, const std::optional<std::string>& slotImage)
    {
        std::string script;
        script += "set pagination off\n";
        script += "set confirm off\n";
        script += "set print thread-events off\n";
        script += "target extended-remote " + gdbTarget + "\n";
        script += "monitor reset\n";
        script += "monitor halt\n";
        if (bootloaderElf)
            script += "load " + *bootloaderElf + "\n";
        script += "load\n";
        // The probe keeps whatever a programmed sector held outside the ELF's sections, and the bootloader hashes the whole image
        if (slotImage)
            script += "restore " + *slotImage + " binary (long)&__application_slot_start\n";
        script += "compare-sections\n";
        script += "monitor reset\n";
        script += "monitor go\n";
        script += "disconnect\n";
        script += "quit\n";
        return script;
    }

    std::string ResetScript(const std::string& gdbTarget)
    {
        std::string script;
        script += "set pagination off\n";
        script += "set confirm off\n";
        script += "target extended-remote " + gdbTarget + "\n";
        script += "monitor reset\n";
        script += "monitor go\n";
        script += "disconnect\n";
        script += "quit\n";
        return script;
    }

    std::optional<std::string> SlotImageOf(const std::string& firmwareElf)
    {
        const auto image = std::filesystem::path{ firmwareElf }.replace_extension(".image.bin");
        if (!std::filesystem::exists(image))
            return std::nullopt;
        return image.string();
    }
}
