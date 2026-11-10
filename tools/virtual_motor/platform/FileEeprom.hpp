#pragma once

#include "hal/interfaces/Eeprom.hpp"
#include <array>
#include <cstdint>
#include <string>

namespace virtual_motor
{
    class FileEeprom
        : public hal::Eeprom
    {
    public:
        static constexpr uint32_t size{ 512 };

        explicit FileEeprom(std::string path);

        uint32_t Size() const override;
        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void Erase(infra::Function<void()> onDone) override;

    private:
        void Load();
        void Store() const;
        static void Complete(const infra::Function<void()>& onDone);

        std::string path;
        std::array<uint8_t, size> memory{};
    };
}
