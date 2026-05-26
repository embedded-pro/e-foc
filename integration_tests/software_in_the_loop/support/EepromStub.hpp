#pragma once

#include "hal/interfaces/Eeprom.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <array>

namespace integration
{
    class EepromStub
        : public hal::Eeprom
    {
    public:
        static constexpr uint32_t stubSize = 512;

        EepromStub();

        uint32_t Size() const override;
        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void Erase(infra::Function<void()> onDone) override;

        // Test-only: defer the next NVM erase completion (delivered via WriteBuffer) instead of calling it immediately.
        void DeferNextErase();
        // Complete the deferred NVM erase completion (WriteBuffer callback). Must only be called after DeferNextErase().
        void CompleteDeferredErase();

    private:
        std::array<uint8_t, stubSize> storage;
        bool deferNextErase{ false };
        infra::Function<void()> deferredEraseCallback;
    };
}
