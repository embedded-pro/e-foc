#pragma once

#include "hal/interfaces/Eeprom.hpp"
#include "hal/interfaces/I2c.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include <array>
#include <chrono>
#include <cstdint>

namespace application
{
    // 24xx01 to 24xx16: one word-address byte, and the address bits above it select a 256-byte block in the device address.
    // A transfer the device does not acknowledge still completes, reading erased bytes, so the caller's CRC sees the fault.
    class Eeprom24xx
        : public hal::Eeprom
    {
    public:
        static constexpr uint32_t blockSize{ 256 };
        static constexpr uint32_t maxPageSize{ 16 };
        static constexpr uint32_t maxSize{ 8 * blockSize };

        struct Config
        {
            uint8_t deviceAddress{ 0x50 };
            uint32_t size{ 512 };
            uint32_t pageSize{ 16 };
            infra::Duration writeCycleTime{ std::chrono::milliseconds(5) };
        };

        Eeprom24xx(hal::I2cMaster& master, const Config& config);

        uint32_t Size() const override;
        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void Erase(infra::Function<void()> onDone) override;

    private:
        hal::I2cAddress DeviceAddress(uint32_t address) const;
        void StartWrite(uint32_t address, uint32_t size, const infra::Function<void()>& onDone);
        void WriteNextPage();
        void PageWritten(hal::Result result);
        infra::ByteRange CurrentChunk() const;
        void ReadNextBlock();
        void WordAddressSent(hal::Result result);
        void ChunkReceived(hal::Result result);

    private:
        hal::I2cMaster& master;
        Config config;
        infra::ConstByteRange source;
        infra::ByteRange destination;
        bool erasing{ false };
        uint32_t address{ 0 };
        uint32_t remaining{ 0 };
        std::array<uint8_t, 1 + maxPageSize> frame{};
        infra::TimerSingleShot writeCycle;
        infra::AutoResetFunction<void()> onDone;
    };
}
