#pragma once

#include "core/bootloader/ApplicationInstaller.hpp"
#include "core/bootloader/ImageFormat.hpp"
#include "core/bootloader/StatusJournal.hpp"
#include "hal/synchronous_interfaces/SynchronousFlash.hpp"
#include "infra/util/ByteRange.hpp"
#include "upgrade/boot_loader/Decryptor.hpp"
#include "upgrade/boot_loader/Verifier.hpp"

namespace bootloader
{
    class ApplicationStarter
    {
    public:
        virtual void Start(uint32_t slotBegin) = 0;
        virtual void Halt() = 0;

    protected:
        ~ApplicationStarter() = default;
    };

    class BootSequence
    {
    public:
        struct Dependencies
        {
            hal::SynchronousFlash& packFlash;
            StatusJournal& journal;
            ApplicationInstaller& installer;
            application::Decryptor& decryptor;
            const application::Verifier& verifier;
            const char* productName;
            infra::ConstByteRange slotMemory;
            TargetLayout layout;
            TargetIdentity identity;
            ApplicationStarter& starter;
        };

        explicit BootSequence(const Dependencies& dependencies);

        void Run();

    private:
        bool InstallRequested() const;
        void Install();

        Dependencies dependencies;
    };
}
