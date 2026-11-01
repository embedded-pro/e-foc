#include "core/bootloader/BootSequence.hpp"
#include "core/bootloader/ApplicationCheck.hpp"
#include "core/bootloader/JournalPackStatus.hpp"
#include <array>

namespace bootloader
{
    BootSequence::BootSequence(const Dependencies& dependencies)
        : dependencies(dependencies)
    {}

    void BootSequence::Run()
    {
        if (InstallRequested())
            Install();

        if (ApplicationIsStartable(dependencies.slotMemory, dependencies.layout, dependencies.identity))
            dependencies.starter.Start(dependencies.layout.slotBegin);
        else
            dependencies.starter.Halt();
    }

    bool BootSequence::InstallRequested() const
    {
        const auto status = dependencies.journal.Status();
        return status == application::UpgradePackStatus::readyToDeploy || status == application::UpgradePackStatus::deployStarted;
    }

    void BootSequence::Install()
    {
        JournalPackLoader loader{ dependencies.packFlash, dependencies.productName, dependencies.journal };
        if (!loader.Load(dependencies.decryptor, dependencies.verifier))
            return;

        std::array<application::ImageUpgrader*, 1> upgraders{ &dependencies.installer };
        JournalPackUpgrader upgrader{ dependencies.packFlash, dependencies.journal };
        upgrader.UpgradeFromImages(infra::MakeRange(upgraders));
    }
}
