#pragma once

#include "hal/interfaces/Eeprom.hpp"
#include "infra/util/Function.hpp"
#include <array>
#include <cstdint>

namespace application
{
    class SemihostingEeprom
        : public hal::Eeprom
    {
    public:
        static constexpr uint32_t storageSize = 256;

        explicit SemihostingEeprom(const char* filePath);

        uint32_t Size() const override;
        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address,
            infra::Function<void()> onDone) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address,
            infra::Function<void()> onDone) override;
        void Erase(infra::Function<void()> onDone) override;

    private:
        void LoadFromFile();
        void FlushToFile() const;

        const char* const filePath;
        std::array<uint8_t, storageSize> storage;
    };
}
