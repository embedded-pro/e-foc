#include "core/platform_abstraction/Eeprom24xx.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace application
{
    Eeprom24xx::Eeprom24xx(hal::I2cMaster& master, const Config& config)
        : master(master)
        , config(config)
    {
        really_assert(config.size != 0 && config.size <= maxSize);
        really_assert(config.pageSize != 0 && config.pageSize <= maxPageSize);
    }

    uint32_t Eeprom24xx::Size() const
    {
        return config.size;
    }

    void Eeprom24xx::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        source = buffer;
        erasing = false;
        StartWrite(address, static_cast<uint32_t>(buffer.size()), onDone);
    }

    void Eeprom24xx::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        really_assert(address <= config.size && buffer.size() <= config.size - address);

        this->onDone = onDone;
        this->address = address;
        destination = buffer;
        ReadNextBlock();
    }

    void Eeprom24xx::Erase(infra::Function<void()> onDone)
    {
        source = infra::ConstByteRange();
        erasing = true;
        StartWrite(0, config.size, onDone);
    }

    hal::I2cAddress Eeprom24xx::DeviceAddress(uint32_t address) const
    {
        return hal::I2cAddress(static_cast<uint16_t>(config.deviceAddress | (address / blockSize)));
    }

    void Eeprom24xx::StartWrite(uint32_t address, uint32_t size, const infra::Function<void()>& onDone)
    {
        really_assert(address <= config.size && size <= config.size - address);

        this->onDone = onDone;
        this->address = address;
        remaining = size;
        WriteNextPage();
    }

    void Eeprom24xx::WriteNextPage()
    {
        if (remaining == 0)
        {
            onDone();
            return;
        }

        const auto chunk = std::min(remaining, config.pageSize - address % config.pageSize);
        const auto device = DeviceAddress(address);

        frame[0] = static_cast<uint8_t>(address);
        auto payload = infra::Head(infra::DiscardHead(infra::MakeRange(frame), 1), chunk);
        if (erasing)
            std::fill(payload.begin(), payload.end(), 0xff);
        else
        {
            std::copy_n(source.begin(), chunk, payload.begin());
            source = infra::DiscardHead(source, chunk);
        }

        address += chunk;
        remaining -= chunk;

        master.SendData(device, infra::Head(infra::MakeRange(frame), chunk + 1), hal::Action::stop, [this](hal::Result result, uint32_t)
            {
                PageWritten(result);
            });
    }

    // The device ignores the bus until its internal write ends; a page it did not acknowledge started no write cycle
    void Eeprom24xx::PageWritten(hal::Result result)
    {
        if (result != hal::Result::complete)
        {
            WriteNextPage();
            return;
        }

        writeCycle.Start(config.writeCycleTime, [this]()
            {
                WriteNextPage();
            });
    }

    infra::ByteRange Eeprom24xx::CurrentChunk() const
    {
        return infra::Head(destination, std::min<std::size_t>(destination.size(), blockSize - address % blockSize));
    }

    void Eeprom24xx::ReadNextBlock()
    {
        if (destination.empty())
        {
            onDone();
            return;
        }

        frame[0] = static_cast<uint8_t>(address);

        master.SendData(DeviceAddress(address), infra::Head(infra::MakeRange(frame), 1), hal::Action::repeatedStart, [this](hal::Result result, uint32_t)
            {
                WordAddressSent(result);
            });
    }

    void Eeprom24xx::WordAddressSent(hal::Result result)
    {
        if (result != hal::Result::complete)
        {
            ChunkReceived(result);
            return;
        }

        master.ReceiveData(DeviceAddress(address), CurrentChunk(), hal::Action::stop, [this](hal::Result result)
            {
                ChunkReceived(result);
            });
    }

    void Eeprom24xx::ChunkReceived(hal::Result result)
    {
        const auto chunk = CurrentChunk();

        if (result != hal::Result::complete)
            std::fill(chunk.begin(), chunk.end(), 0xff);

        address += static_cast<uint32_t>(chunk.size());
        destination = infra::DiscardHead(destination, chunk.size());
        ReadNextBlock();
    }
}
