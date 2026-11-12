#pragma once

#include <optional>
#include <string>

namespace hil
{
    std::string FlashScript(const std::string& gdbTarget, const std::optional<std::string>& bootloaderElf, const std::optional<std::string>& slotImage);
    std::string ResetScript(const std::string& gdbTarget);
    std::optional<std::string> SlotImageOf(const std::string& firmwareElf);
}
